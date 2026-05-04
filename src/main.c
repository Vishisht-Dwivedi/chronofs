//code for daemon process... runs in background and listens to listener process messages
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <stdbool.h>
#include <limits.h>
#include <time.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/un.h>
//define socket file
#ifndef CONNECTION_H
#define CONNECTION_H
#define SOCKET_NAME "/tmp/chronofs.socket"
#define SOCKET_BUFFER_SIZE 12
#endif

#define MAX_PATH_LENGTH 4096

//mapping wd to its directory.. used for storing listeners on multiple directories
struct wd_map {
    int wd;
    char path[MAX_PATH_LENGTH];
    bool active;
};
//making a struct for handling all data abt execution
struct chronofs_data {
    struct wd_map *wd_map;
    int inotify_fd;
    FILE *log_file;
    char root[MAX_PATH_LENGTH];
};
// resizable array size
int wd_arr_size = 128;
int wd_top = 0;
// utility to copy array... making a new array twice the size and returning ptr to it..
struct wd_map* wd_map_extend(struct wd_map *arr);
//recurse through all directories and attach listeners
int dfs(char *base_dir, struct wd_map **map, int fd);

//init func
struct chronofs_data* init(char *watch_dir);

int main(int argc, char *argv[])
{
    // two args -> first to call binary.. second to pass path
    if (argc != 2)
    {
        printf("usage: %s <directory-to-watch>\n", argv[0]);
        return -1;
    }
    struct chronofs_data *global_data = init(argv[1]);
    if (global_data == NULL)
    {
        perror("init failed\n");
        return -1;
    }
    //buffer where fd will be read and stored
    char buffer[4096];
    time_t now;
    time(&now);
    // infinite loop
    while (true)
    {
        //size of file descriptor buffer received from inotify_init
        ssize_t len = read(global_data->inotify_fd, buffer, sizeof(buffer));
        if(len < 0)
        {
            perror("Error during reading the file descriptor\n");
            return -1;
        }
        //reading all changes
        for (int i = 0; i < len;)
        {
            //cast buffer to event pointer
            struct inotify_event *event = (struct inotify_event *)&buffer[i];
            //skip log file changes
            if(event->len>0 && strcmp(event->name, "current.log") != 0){
                time(&now);
                //if creation event
                if (IN_CREATE & event->mask)
                {
                    fprintf(global_data->log_file,"%ld Created %s\n", now, event->name);
                    fflush(global_data->log_file);
                    //if directory is created
                    if(event->mask & IN_ISDIR)
                    {
                        //find parent wd
                        for (int j = 0; j < wd_top; j++)
                        {
                            if(global_data->wd_map[j].wd == event->wd){
                                char new_dir_path[MAX_PATH_LENGTH];
                                strcpy(new_dir_path, global_data->wd_map[j].path);
                                int last = strlen(new_dir_path);
                                if (last > 0 && new_dir_path[last - 1] != '/') {
                                    new_dir_path[last] = '/';
                                    new_dir_path[last + 1] = '\0';
                                }
                                //add generated path name to parent path
                                strcat(new_dir_path, event->name);
                                int new_wd = inotify_add_watch(global_data->inotify_fd, new_dir_path, IN_ALL_EVENTS);
                                if (new_wd == -1)
                                {
                                    perror("Dynamic directory watch event listener failure\n");
                                    return -1;
                                }
                                struct wd_map new_dir;
                                new_dir.wd = new_wd;
                                new_dir.active = true;
                                strcpy(new_dir.path, new_dir_path);

                                // resize if needed
                                if(wd_top == wd_arr_size)
                                {
                                    global_data->wd_map = wd_map_extend(global_data->wd_map);
                                    if(global_data->wd_map == NULL)
                                    {
                                        perror("Error while allocating new memory to map within dynamic wd allocation\n");
                                        return -1;
                                    }
                                }
                                global_data->wd_map[wd_top++] = new_dir;
                                break;
                            }
                        }
                    }
                }
                //handle deletes
                if (IN_DELETE & event->mask)
                {
                    fprintf(global_data->log_file ,"%ld Deleted %s\n", now, event->name);
                    fflush(global_data->log_file);
                    //if directory delete
                    if(event->mask & IN_ISDIR){
                        //find parent wd
                        for (int j = 0; j < wd_top; j++)
                        {
                            if (global_data->wd_map[j].wd == event->wd)
                            {
                                //get parent path
                                char deleted_path[MAX_PATH_LENGTH];
                                strcpy(deleted_path, global_data->wd_map[j].path);
                                int last = strlen(deleted_path);
                                if(last>0 && deleted_path[last-1]!='/')
                                {
                                    deleted_path[last] = '/';
                                    deleted_path[last + 1] = '\0';
                                }
                                //get deleted child's path
                                strcat(deleted_path, event->name);
                                //find child descriptor and remove it from watch list
                                for (int k = 0; k < wd_top; k++)
                                {
                                    if(strcmp(global_data->wd_map[k].path, deleted_path) == 0)
                                    {
                                        inotify_rm_watch(global_data->inotify_fd, global_data->wd_map[k].wd);
                                        global_data->wd_map[k].active = false;
                                        global_data->wd_map[k] = global_data->wd_map[wd_top - 1];
                                        wd_top--;
                                        break;
                                    }
                                }
                                break;
                            }
                        }
                    }
                } 
                if (IN_CLOSE_WRITE & event->mask)
                {
                    fprintf(global_data->log_file, "%ld Modified %s\n", now, event->name);
                    fflush(global_data->log_file);
                } 
                if (event->mask & IN_MOVED_FROM)
                {
                    fprintf(global_data->log_file, "%ld Moved from %s\n", now, event->name);
                    fflush(global_data->log_file);
                }
                if (event->mask & IN_MOVED_TO)
                {
                    fprintf(global_data->log_file, "%ld Moved to %s\n", now, event->name);
                    fflush(global_data->log_file);
                }
            }
            //event struct + variable length of name
            i += sizeof(struct inotify_event) + event->len;
        }
    }
    return 0;
}

struct wd_map* wd_map_extend(struct wd_map *arr) {
    int newN = wd_arr_size * 2;
    struct wd_map *newArr = calloc(newN, sizeof(struct wd_map));
    if(newArr == NULL){
        perror("Error while allocating memory during array extension\n");
        return NULL;
    }
    int j = 0;
    wd_top = 0;
    for (int i = 0; i < wd_arr_size; i++)
    {
        if(arr[i].active){
            newArr[j++] = arr[i];
            wd_top++;
        }
    }
    wd_arr_size = newN;
    free(arr);
    return newArr;
}

int dfs(char *base_dir, struct wd_map **map, int fd) {
    //open directory
    DIR *dirp = opendir(base_dir);
    if (dirp == NULL)
        return 0;
    //struct to handle entries
    struct dirent *dir_entry;
    //open an entry.. readdir automatically points to next one.. do this until null reached
    while ((dir_entry = readdir(dirp)) != NULL)
    {
        // skip . and ..
        if (strcmp(dir_entry->d_name, ".") != 0 && strcmp(dir_entry->d_name, "..") != 0) 
        {
            //if directory
            if (dir_entry->d_type == DT_DIR) {
                //path handling
                char watch_dir[MAX_PATH_LENGTH];

                strcpy(watch_dir, base_dir);
                int last = strlen(watch_dir);
                //if last path didnt have / add it
                if (last > 0 && watch_dir[last - 1] != '/') {
                    watch_dir[last] = '/';
                    watch_dir[last + 1] = '\0';
                }

                strcat(watch_dir, dir_entry->d_name);
                // attach listener and gets its descriptor
                int wd = inotify_add_watch(fd, watch_dir, IN_ALL_EVENTS);
                if (wd < 0) {
                    perror("Error occured during add_watch");
                    closedir(dirp);
                    return -1;
                }
                //add wd to map
                struct wd_map mp;
                mp.wd = wd;
                mp.active = true;
                strcpy(mp.path, watch_dir);

                // resize if needed
                if (wd_top == wd_arr_size) {
                    struct wd_map *new_map = wd_map_extend(*map);
                    if (new_map == NULL) {
                        closedir(dirp);
                        return -1;
                    }
                    *map = new_map;
                }

                (*map)[wd_top++] = mp;

                // recurse
                if (dfs(watch_dir, map, fd) == -1) {
                    closedir(dirp);
                    return -1;
                }
            }
        }
    }

    if (closedir(dirp) != 0) {
        perror("Error while closing directory");
        return -1;
    }

    return 0;
}
struct chronofs_data* init(char *watch_dir){
    //assumed max path length
    char cwd[MAX_PATH_LENGTH];
    //getting the directory from where the file was called
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        printf("Current working dir: %s\n", cwd);
    }
    else
    {
        perror("getcwd() error");
        return NULL;
    }
    int fd = inotify_init();
    //check for error
    if(fd < 0)
    {
        perror("Error occured during initialization\n");
        return NULL;
    }
    struct wd_map *map = calloc(wd_arr_size, sizeof(struct wd_map));
    if(map == NULL){
        perror("Error during memory allocation to wd_map\n");
        return NULL;
    }
    // adding listener to watch directory
    int wd = inotify_add_watch(fd, watch_dir, IN_ALL_EVENTS);
    if (wd < 0)
    {
        perror("Error occured during add_watch");
        return NULL;
    }
    printf("Watching directory: %s\n", watch_dir);
    // store root in map
    map[wd_top].wd = wd;
    map[wd_top].active = true;
    strcpy(map[wd_top].path, watch_dir);
    wd_top++;
    // build recursive watchers
    if (dfs(watch_dir, &map, fd) == -1) {
        perror("DFS failed");
        return NULL;
    }
    //setting up a folder to track current logs within the working directory
    FILE *current_log_ptr = fopen("current.log", "a");
    if(current_log_ptr == NULL){
        perror("Error while accessing log file\n");
        return NULL;
    }
    //time when directory started being observed
    time_t now;
    time(&now);
    fprintf(current_log_ptr, "Watchtime started at: %s\n", ctime(&now));
    fflush(current_log_ptr);
    //copying data to global data
    struct chronofs_data *global_data = malloc(sizeof(struct chronofs_data));
    global_data->inotify_fd = fd;
    global_data->log_file = current_log_ptr;
    strcpy(global_data->root, watch_dir);
    global_data->wd_map = map;
    return global_data;
}