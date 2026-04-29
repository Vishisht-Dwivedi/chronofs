#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <stdbool.h>
int main(int argc, char *argv[]) {
    if(argc != 2)
    {
        printf("usage: %s <directory-to-watch>\n", argv[0]);
        return -1;
    }
    char *watch_dir = argv[1];
    int fd = inotify_init();
    if(fd < 0)
    {
        perror("Error occured during initialization\n");
        return -1;
    }
    inotify_add_watch(fd, watch_dir, IN_CLOSE_WRITE | IN_DELETE);
    if(fd < 0)
    {
        perror("Error occured during add_watch\n");
        return -1;
    }
    printf("Watching directory: %s\n", argv[1]);
    char buffer[4096];
    while (true)
    {
        ssize_t len = read(fd, buffer, sizeof(buffer));
        if(len < 0)
        {
            perror("Error during reading the file descriptor\n");
            return -1;
        }
        for (int i = 0; i < len;)
        {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];
            if(IN_CLOSE_WRITE & event->mask)
            {
                printf("Created %s\n", event->name);
            } else if(IN_DELETE & event->mask)
            {
                printf("Deleted %s\n", event->name);
            }
            i += sizeof(struct inotify_event) + event->len;
        }
    }
    return 0;
}