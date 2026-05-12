#define _DEFAULT_SOURCE
#include "protocol.h"
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include "utilities.h"
#include "../fs/events.h"

int THREAD_ARRAY_SIZE = 10;
int THREAD_ARRAY_TOP = 0;

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

int thread_map_extend(struct watcher_thread **thread_array) 
{
    THREAD_ARRAY_SIZE *= 2;
    struct watcher_thread *newArr = calloc(THREAD_ARRAY_SIZE, sizeof(struct watcher_thread));
    if(newArr == NULL){
        perror("Error while allocating memory during array extension\n");
        return -1;
    }
    int old_top = THREAD_ARRAY_TOP;
    THREAD_ARRAY_TOP = 0;
    for (int i = 0; i < old_top; i++)
    {
        if((*thread_array)[i].active)
        {
            newArr[THREAD_ARRAY_TOP++] = (*thread_array)[i];
        }
    }
    free(*thread_array);
    *thread_array = newArr;
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

//closes files and file descriptors
void cleanup_watcher(void *arg)
{
    struct chronofs_data *data = arg;
    close(data->inotify_fd);
    fclose(data->log_file);
    free(data->wd_map);
    free(data);
}
void *watcher_worker(void *arg) 
{
    struct chronofs_data *data = arg;
    printf("watcher thread spawned\n");
    pthread_cleanup_push(cleanup_watcher, data);
    if (eventController(data) == -1)
    {
        perror("Exiting thread due to error\n");
    }
    pthread_cleanup_pop(1);
    return NULL;
}