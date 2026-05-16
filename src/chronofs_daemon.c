//code for daemon process... runs in background and listens to listener process messages
#define _DEFAULT_SOURCE
#include "common/protocol.h"
#include "fs/events.h"
#include "common/utilities.h"
#include "fs/watcher.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include "fs/socket_events.h"

int main()
{
    int socket_fd = socket_init();
    if(socket_fd == -1)
    {
        perror("Initialisation error during socket allocations\n");
        return -1;
    }
    if(socket_event_loop(socket_fd) == -1)
    {
        perror("Fatal error in socket event loop\n");
        return -1;
    }
    return 0;
}
