#include "watcher.h"

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
    char log_path[MAX_PATH_LENGTH];
    strcpy(log_path, watch_dir);
    int last = strlen(log_path);
    if(last > 0 && log_path[last - 1] != '/')
    {
        log_path[last] = '/';
        log_path[last + 1] = '\0';
    }
    strcat(log_path, "current.log");
    FILE *current_log_ptr = fopen(log_path, "a");
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
