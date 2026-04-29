#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <stdbool.h>
#include <limits.h>
#include <time.h>

int main(int argc, char *argv[]) {
    // two args -> first to call binary.. second to pass path
    if (argc != 2)
    {
        printf("usage: %s <directory-to-watch>\n", argv[0]);
        return -1;
    }
    //assumed max path length
    char cwd[4096];
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
    //adding listener to watch directory
    int wd = inotify_add_watch(fd, watch_dir, IN_ALL_EVENTS);
    if (wd < 0)
    {
        perror("Error occured during add_watch");
        return -1;
    }
    printf("Watching directory: %s\n", argv[1]);

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
                now = time(&now);
                if (IN_CREATE & event->mask)
                {
                    fprintf(current_log_ptr ,"%ld Created %s\n", now, event->name);
                    fflush(current_log_ptr);
                }
                if (IN_DELETE & event->mask)
                {
                    fprintf(current_log_ptr ,"%ld Deleted %s\n", now, event->name);
                    fflush(current_log_ptr);
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