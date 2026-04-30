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

#define MAX_PATH_LENGTH 4096

//mapping wd to its directory.. used for storing listeners on multiple directories
struct wd_map {
    int wd;
    char path[MAX_PATH_LENGTH];
    bool active;
};
// resizable array size
int wd_arr_size = 128;
int wd_top = 0;
// utility to copy array... making a new array twice the size and returning ptr to it..
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
};
int dfs(char *base_dir, struct wd_map **map, int fd) {
    DIR *dirp = opendir(base_dir);
    if (dirp == NULL)
        return 0;
    struct dirent *dir_entry;

    while ((dir_entry = readdir(dirp)) != NULL)
    {
        if (strcmp(dir_entry->d_name, ".") != 0 && strcmp(dir_entry->d_name, "..") != 0) 
        {
            if (dir_entry->d_type == DT_DIR) {
                char watch_dir[MAX_PATH_LENGTH];

                strcpy(watch_dir, base_dir);
                int last = strlen(watch_dir);

                if (last > 0 && watch_dir[last - 1] != '/') {
                    watch_dir[last] = '/';
                    watch_dir[last + 1] = '\0';
                }

                strcat(watch_dir, dir_entry->d_name);

                int wd = inotify_add_watch(fd, watch_dir, IN_ALL_EVENTS);
                if (wd < 0) {
                    perror("Error occured during add_watch");
                    closedir(dirp);
                    return -1;
                }

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


int main(int argc, char *argv[])
{
    // two args -> first to call binary.. second to pass path
    if (argc != 2)
    {
        printf("usage: %s <directory-to-watch>\n", argv[0]);
        return -1;
    }
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
        return 1;
    }
    //watching the directory passed as argument
    char *watch_dir = argv[1];
    int fd = inotify_init();
    //check for error
    if(fd < 0)
    {
        perror("Error occured during initialization\n");
        return -1;
    }
    
    struct wd_map *map = calloc(wd_arr_size, sizeof(struct wd_map));
    if(map == NULL){
        perror("Error during memory allocation to wd_map\n");
        return -1;
    }

    // adding listener to watch directory
    int wd = inotify_add_watch(fd, watch_dir, IN_ALL_EVENTS);
    if (wd < 0)
    {
        perror("Error occured during add_watch");
        return -1;
    }
    printf("Watching directory: %s\n", argv[1]);

    // store root in map
    map[wd_top].wd = wd;
    map[wd_top].active = true;
    strcpy(map[wd_top].path, watch_dir);
    wd_top++;

    // build recursive watchers
    if (dfs(watch_dir, &map, fd) == -1) {
        perror("DFS failed");
        return -1;
    }

    //setting up a folder to track current logs within the working directory
    FILE *current_log_ptr = fopen("current.log", "a");
    if(current_log_ptr == NULL){
        perror("Error while accessing log file\n");
        return -1;
    }
    //time when directory started being observed
    time_t now;
    time(&now);
    fprintf(current_log_ptr, "Watchtime started at: %s\n", ctime(&now));
    fflush(current_log_ptr);
    //buffer where fd will be read and stored
    char buffer[4096];
    //infinite loop
    while (true)
    {
        //size of file descriptor buffer received from inotify_init
        ssize_t len = read(fd, buffer, sizeof(buffer));
        if(len < 0)
        {
            perror("Error during reading the file descriptor\n");
            return -1;
        }
        //reading all changes
        for (int i = 0; i < len;)
        {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];
            //skip log file changes
            if(event->len>0 && strcmp(event->name, "current.log") != 0){
                time(&now);
                if (IN_CREATE & event->mask)
                {
                    fprintf(current_log_ptr ,"%ld Created %s\n", now, event->name);
                    fflush(current_log_ptr);
                    if(event->mask & IN_ISDIR)
                    {
                        for (int j = 0; j < wd_top; j++)
                        {
                            if(map[j].wd == event->wd){
                                char new_dir_path[MAX_PATH_LENGTH];
                                strcpy(new_dir_path, map[j].path);
                                int last = strlen(new_dir_path);
                                if (last > 0 && new_dir_path[last - 1] != '/') {
                                    new_dir_path[last] = '/';
                                    new_dir_path[last + 1] = '\0';
                                }
                                strcat(new_dir_path, event->name);
                                int new_wd = inotify_add_watch(fd, new_dir_path, IN_ALL_EVENTS);
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
                                    map = wd_map_extend(map);
                                    if(map == NULL)
                                    {
                                        perror("Error while allocating new memory to map within dynamic wd allocation\n");
                                        return -1;
                                    }
                                }
                                map[wd_top++] = new_dir;
                                break;
                            }
                        }
                    }
                }
                if (IN_DELETE & event->mask)
                {
                    fprintf(current_log_ptr ,"%ld Deleted %s\n", now, event->name);
                    fflush(current_log_ptr);
                    if(event->mask & IN_ISDIR){
                        for (int j = 0; j < wd_top; j++)
                        {
                            if (map[j].wd == event->wd)
                            {
                                char deleted_path[MAX_PATH_LENGTH];
                                strcpy(deleted_path, map[j].path);
                                int last = strlen(deleted_path);
                                if(last>0 && deleted_path[last-1]!='/')
                                {
                                    deleted_path[last] = '/';
                                    deleted_path[last + 1] = '\0';
                                }
                                strcat(deleted_path, event->name);
                                for (int k = 0; k < wd_top; k++)
                                {
                                    if(strcmp(map[k].path, deleted_path) == 0)
                                    {
                                        inotify_rm_watch(fd, map[k].wd);
                                        map[k].active = false;
                                        map[k] = map[wd_top - 1];
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
                    fprintf(current_log_ptr ,"%ld Modified %s\n", now, event->name);
                    fflush(current_log_ptr);
                } 
                if (event->mask & IN_MOVED_FROM)
                {
                    fprintf(current_log_ptr ,"%ld Moved from %s\n", now, event->name);
                    fflush(current_log_ptr);
                }
                if (event->mask & IN_MOVED_TO)
                {
                    fprintf(current_log_ptr ,"%ld Moved to %s\n", now, event->name);
                    fflush(current_log_ptr);
                }
            }
            //event struct + variable length of name
            i += sizeof(struct inotify_event) + event->len;
        }
    }
    return 0;
}