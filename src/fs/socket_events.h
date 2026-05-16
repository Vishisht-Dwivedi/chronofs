#ifndef SOCKET_EVENTS_H
#define SOCKET_EVENTS_H
#include "../common/protocol.h"
#include "events.h"
#include "../common/utilities.h"
#include "watcher.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#define MAX_BUFFER_SIZE 8192
extern struct watcher_thread *watcher_thread_array;
int socket_init();
int socket_event_loop(int socket_fd);
int watch(struct chronofs_packet *packet);
int unwatch(struct chronofs_packet *packet);
int commit(struct chronofs_packet *packet);
int recursive_copy(char *curr_dir, char *target_dir);
#endif