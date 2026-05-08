#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/un.h>
#include <unistd.h>
#define SOCKET_NAME "/tmp/chronofs.socket"
#define MAX_PATH_LENGTH 4096
//defining packet struct
struct chronofs_packet
{
    int type;
    char path[MAX_PATH_LENGTH];
};
//request type enum
enum request_type
{
    WATCH,
    UNWATCH,
    COMMIT
};

int main(int argc, char *argv[]) {
    //if less than three arguments... exit
    if(argc != 3){
        perror("Usage: chronofs watch directory\n");
        return -1;
    }
    //instance of packet struct
    struct chronofs_packet packet;
    //switching based on values of argv[1]
    if (strcmp(argv[1], "watch") == 0)
    {
        packet.type = WATCH;
    }
    else if (strcmp(argv[1], "unwatch") == 0)
    {
        packet.type = UNWATCH;
    } 
    else if (strcmp(argv[1], "commit") == 0)
    {
        packet.type = COMMIT;
    } 
    else 
    {
        printf("Invalid usage: %s %s %s\n", argv[0], argv[1], argv[2]);
        return -1;
    }
    strcpy(packet.path, argv[2]);
    //Address struct
    struct sockaddr_un addr;
    //clear unnecessary stuff
    memset(&addr, 0, sizeof(addr));
    //local family
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_NAME, sizeof(addr.sun_path) - 1);
    //setup socket
    int socket_fd = socket(AF_LOCAL, SOCK_SEQPACKET, 0);
    if(socket_fd == -1)
    {
        perror("Error while setting up a socket\n");
        return -1;
    }
    if (connect(socket_fd, (const struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("Error while setting up a connection over the socket\n");
        return -1;
    }
    //doesnt throw since it itself is a throwing point
    write(socket_fd, &packet, sizeof(packet));
    //close the socket
    close(socket_fd);
    return 0;
}