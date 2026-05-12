//code for daemon process... runs in background and listens to listener process messages
#define _DEFAULT_SOURCE
#include "common/protocol.h"
#include "fs/events.h"
#include "common/utilities.h"
#include "fs/watcher.h"

#include <sys/socket.h>
#include <sys/un.h>

int main()
{
    struct watcher_thread *thread_array = calloc(THREAD_ARRAY_SIZE, sizeof(struct watcher_thread));
    // local socket for ipc
    int socket_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if(socket_fd == -1){
        perror("Socket allocation failure\n");
        return -1;
    }
    //struct to handle socket location data
    struct sockaddr_un socket_addr;
    //remove unnecessary data from struct
    memset(&socket_addr, 0, sizeof(socket_addr));
    //set family to unix descriptors
    socket_addr.sun_family = AF_UNIX;
    strncpy(socket_addr.sun_path, SOCKET_NAME, sizeof(socket_addr.sun_path) - 1);
    //unlinking old unexited connections
    unlink(SOCKET_NAME);
    // binding call
    int ret = bind(socket_fd, (const struct sockaddr *)&socket_addr, sizeof(socket_addr));
    if(ret == -1){
        perror("Socket binding failure\n");
        return -1;
    }
    //listen for data on the socket with a queue of 20...
    ret = listen(socket_fd, 20);
    if(ret == -1){
        perror("Socket listening failure\n");
        return -1;
    }
    int data_socket;
    ssize_t r;
    while (true)
    {
        data_socket = accept(socket_fd, NULL, NULL);
        if(data_socket == -1) {
            perror("Failure while socket allocation on accept\n");
            return -1;
        }
        struct chronofs_packet packet;
        while (true)
        {
            r = read(data_socket, &packet, sizeof(packet));
            if(r == 0)
            {
                close(data_socket);
                break;
            } 
            else if(r == -1)
            {
                perror("Error while reading from socket into buffer\n");
                return -1;
            }

            switch(packet.type)
            {
                case WATCH:
                    struct watcher_thread thread_data;
                    thread_data.active = true;
                    thread_data.data = init(packet.path);
                    if(thread_data.data == NULL)
                    {
                        perror("init failed\n");
                        return -1;
                    }
                    pthread_create(&thread_data.thread_id, NULL, watcher_worker, thread_data.data);
                    if(THREAD_ARRAY_TOP == THREAD_ARRAY_SIZE) 
                    {
                        thread_map_extend(&thread_array);
                    }
                    thread_array[THREAD_ARRAY_TOP++] = thread_data;
                    break;
                case UNWATCH:
                    bool deleted = false;
                    for (int i = 0; i < THREAD_ARRAY_TOP; i++)
                    {
                        if(strcmp(thread_array[i].data->root, packet.path) == 0) 
                        {
                            deleted = true;
                            pthread_cancel(thread_array[i].thread_id);
                            fprintf(thread_array[i].data->log_file, "Detaching watcher\n");
                            printf("Removing watcher thread for dir: %s\n", packet.path);
                            thread_array[i] = thread_array[THREAD_ARRAY_TOP - 1];
                            thread_array[THREAD_ARRAY_TOP].active = false;
                            THREAD_ARRAY_TOP--;
                            break;
                        }
                    }
                    if(!deleted) 
                    {
                        printf("Directory not found to unwatch call\n");
                    }
                    break;
                }
        }
    }
    return 0;
}
