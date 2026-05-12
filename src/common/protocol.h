#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>

#define SOCKET_NAME "/tmp/chronofs.socket"
#define SOCKET_BUFFER_SIZE 4096
#define MAX_PATH_LENGTH 4096

enum request_type
{
    WATCH,
    UNWATCH,
    COMMIT
};

struct chronofs_packet
{
    int type;
    char path[MAX_PATH_LENGTH];
};

// mapping wd to its directory.. used for storing listeners on multiple directories
struct wd_map 
{
    int wd;
    char path[MAX_PATH_LENGTH];
    bool active;
};
// execution structs
struct chronofs_data 
{
    struct wd_map *wd_map;
    int inotify_fd;
    FILE *log_file;
    char root[MAX_PATH_LENGTH];
    int wd_top;
    int wd_arr_size;
};
struct watcher_thread {
    pthread_t thread_id;
    struct chronofs_data *data;
    bool active;
};


#endif