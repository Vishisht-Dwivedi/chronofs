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
    int wd_top;
    int wd_arr_size;
};
// utility to copy array... making a new array twice the size and returning ptr to it..
int wd_map_extend(struct chronofs_data *data);
//recurse through all directories and attach listeners
int dfs(struct chronofs_data *data, char *curr_dir);

//init func
struct chronofs_data* init(char *watch_dir);

//event controller
int eventController(struct chronofs_data *data);

//event handlers
int createEvent(struct chronofs_data *data, time_t now, struct inotify_event *event);
int deleteEvent(struct chronofs_data *data, time_t now, struct inotify_event *event);
int modifyEvent(struct chronofs_data *data, time_t now, struct inotify_event *event);
int moveEvent(struct chronofs_data *data, time_t now, struct inotify_event *event);

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
    //event control
    if(eventController(global_data) == -1)
        return -1;
    return 0;
}

int wd_map_extend(struct chronofs_data *data) {
    int newN = data->wd_arr_size * 2;
    struct wd_map *newArr = calloc(newN, sizeof(struct wd_map));
    if(newArr == NULL){
        perror("Error while allocating memory during array extension\n");
        return -1;
    }
    int j = 0;
    data->wd_top = 0;
    for (int i = 0; i < data->wd_arr_size; i++)
    {
        if(data->wd_map[i].active){
            newArr[j++] = data->wd_map[i];
            data->wd_top++;
        }
    }
    data->wd_arr_size = newN;
    free(data->wd_map);
    data->wd_map = newArr;
    return 0;
}

int dfs(struct chronofs_data *data, char *curr_dir) {
    //open directory
    DIR *dirp = opendir(curr_dir);
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

                strcpy(watch_dir, curr_dir);
                int last = strlen(watch_dir);
                //if last path didnt have / add it
                if (last > 0 && watch_dir[last - 1] != '/') {
                    watch_dir[last] = '/';
                    watch_dir[last + 1] = '\0';
                }

                strcat(watch_dir, dir_entry->d_name);
                // attach listener and gets its descriptor
                int wd = inotify_add_watch(data->inotify_fd, watch_dir, IN_ALL_EVENTS);
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
                if (data->wd_top == data->wd_arr_size) {
                    if (wd_map_extend(data) == -1)
                    {
                        closedir(dirp);
                        perror("Error while allocating dynamic memory for extended array in dfs\n");
                        return -1;
                    }
                }

                data->wd_map[data->wd_top++] = mp;

                // recurse
                if (dfs(data, watch_dir) == -1) {
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
    int wd_top = 0;
    int wd_arr_size = 128;
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
    global_data->wd_top = wd_top;
    global_data->wd_arr_size = wd_arr_size;
    // build recursive watchers
    if (dfs(global_data, global_data->root) == -1) {
        perror("DFS failed");
        return NULL;
    }
    return global_data;
}

//event controller
int eventController(struct chronofs_data *data) {
    //buffer where fd will be read and stored
    char buffer[4096];
    time_t now;
    time(&now);
    // infinite loop
    while (true)
    {
        //size of file descriptor buffer received from inotify_init
        ssize_t len = read(data->inotify_fd, buffer, sizeof(buffer));
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
                    if(createEvent(data, now, event) == -1)
                        return -1;
                // handle deletes
                if (IN_DELETE & event->mask)
                    if(deleteEvent(data, now, event) == -1)
                        return -1;
                if (IN_CLOSE_WRITE & event->mask)
                    if(modifyEvent(data, now, event) == -1)
                        return -1;
                if ((event->mask & IN_MOVED_FROM) || (event->mask & IN_MOVED_TO))
                    if(moveEvent(data, now, event) == -1)
                        return -1;
            }
            //event struct + variable length of name
            i += sizeof(struct inotify_event) + event->len;
        }
    }
    return 0;
}

//event handlers

int createEvent(struct chronofs_data *data, time_t now, struct inotify_event *event)
{
    fprintf(data->log_file,"%ld Created %s\n", now, event->name);
    fflush(data->log_file);
    //if directory is created
    if(event->mask & IN_ISDIR)
    {
        //find parent wd
        for (int j = 0; j < data->wd_top; j++)
        {
            if(data->wd_map[j].wd == event->wd)
            {
                char new_dir_path[MAX_PATH_LENGTH];
                strcpy(new_dir_path, data->wd_map[j].path);
                int last = strlen(new_dir_path);
                if (last > 0 && new_dir_path[last - 1] != '/') {
                    new_dir_path[last] = '/';
                    new_dir_path[last + 1] = '\0';
                }
                //add generated path name to parent path
                strcat(new_dir_path, event->name);
                int new_wd = inotify_add_watch(data->inotify_fd, new_dir_path, IN_ALL_EVENTS);
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
                if(data->wd_top == data->wd_arr_size)
                {
                    if(wd_map_extend(data) == -1)
                    {
                        perror("Error while allocating new memory to map within dynamic wd allocation\n");
                        return -1;
                    }
                }
                data->wd_map[data->wd_top++] = new_dir;
                break;
            }
        }
    }
    return 0;
}
int deleteEvent(struct chronofs_data *data, time_t now, struct inotify_event *event) 
{
    fprintf(data->log_file ,"%ld Deleted %s\n", now, event->name);
    fflush(data->log_file);
    //if directory delete
    if(event->mask & IN_ISDIR){
        //find parent wd
        for (int j = 0; j < data->wd_top; j++)
        {
            if (data->wd_map[j].wd == event->wd)
            {
                //get parent path
                char deleted_path[MAX_PATH_LENGTH];
                strcpy(deleted_path, data->wd_map[j].path);
                int last = strlen(deleted_path);
                if(last>0 && deleted_path[last-1]!='/')
                {
                    deleted_path[last] = '/';
                    deleted_path[last + 1] = '\0';
                }
                //get deleted child's path
                strcat(deleted_path, event->name);
                //find child descriptor and remove it from watch list
                for (int k = 0; k < data->wd_top; k++)
                {
                    if(strcmp(data->wd_map[k].path, deleted_path) == 0)
                    {
                        inotify_rm_watch(data->inotify_fd, data->wd_map[k].wd);
                        data->wd_map[k].active = false;
                        data->wd_map[k] = data->wd_map[data->wd_top - 1];
                        data->wd_top--;
                        break;
                    }
                }
                break;
            }
        }
    }
    return 0;
}
int modifyEvent(struct chronofs_data *data, time_t now, struct inotify_event *event) {
    fprintf(data->log_file, "%ld Modified %s\n", now, event->name);
    fflush(data->log_file);
    return 0;
}
int moveEvent(struct chronofs_data *data, time_t now, struct inotify_event *event) {
    if (event->mask & IN_MOVED_FROM)
    {
        fprintf(data->log_file, "%ld Moved from %s\n", now, event->name);
        fflush(data->log_file);
    }
    if (event->mask & IN_MOVED_TO)
    {
        fprintf(data->log_file, "%ld Moved to %s\n", now, event->name);
        fflush(data->log_file);
    }
    return 0;
}