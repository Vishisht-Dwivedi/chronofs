#ifndef UTILITIES_H
#define UTILITIES_H

#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>

extern int THREAD_ARRAY_SIZE;
extern int THREAD_ARRAY_TOP;

int wd_map_extend(struct chronofs_data *data);

int thread_map_extend(struct watcher_thread **thread_array);

int dfs(struct chronofs_data *data, char *curr_dir);
// thread handlers
void *watcher_worker(void *arg);
void cleanup_watcher(void *arg);

#endif