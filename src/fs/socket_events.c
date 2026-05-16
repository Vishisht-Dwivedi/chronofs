#define _DEFAULT_SOURCE
#include "socket_events.h"
struct watcher_thread *watcher_thread_array;
// returns socket fd
int socket_init()
{
    watcher_thread_array = calloc(THREAD_ARRAY_SIZE, sizeof(struct watcher_thread));
    // local socket for ipc
    int socket_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if(socket_fd == -1)
    {
        perror("Socket Allocation Failure\n");
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
    //binding call
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
    return socket_fd;
}

//event loop for socket events
int socket_event_loop(int socket_fd)
{
    // socket which is used to accept incoming
    int data_socket;
    ssize_t r;
    //listener loop
    while(true)
    {
        data_socket = accept(socket_fd, NULL, NULL);
        if(data_socket == -1)
        {
            perror("Failure while socket allocation on accept\n");
            return -1;
        }
        struct chronofs_packet packet;
        //worker loop
        while (true)
        {
            //read the predefined struct packet... common to both daemon and listener
            r = read(data_socket, &packet, sizeof(packet));
            if(r == 0)
            {
                close(data_socket);
                break;
            }
            else if (r == -1)
            {
                perror("Error while reading from socket into buffer\n");
                return -1;
            }

            switch(packet.type)
            {
                case WATCH:
                    if(watch(&packet) == -1)
                    {
                        perror("Error in watch socket event\n");
                    }
                    break;
                case UNWATCH:
                    if(unwatch(&packet) == -1)
                    {
                        perror("Error in unwatch socket event\n");
                    }
                    break;
                case COMMIT:
                    if(commit(&packet) == -1)
                    {
                        perror("Error in commit socket event\n");
                    }
                    break;
            }
        }
    }
    return 0;
}

//watcher function
int watch(struct chronofs_packet *packet)
{
    struct watcher_thread thread_data;
    thread_data.active = true;
    thread_data.data = init(packet->path);
    if(thread_data.data == NULL)
    {
        perror("Thread initialisation failed inside event watch event\n");
        return -1;
    }
    pthread_create(&thread_data.thread_id, NULL, watcher_worker, thread_data.data);
    if(THREAD_ARRAY_TOP == THREAD_ARRAY_SIZE) 
    {
        thread_map_extend(&watcher_thread_array);
    }
    watcher_thread_array[THREAD_ARRAY_TOP++] = thread_data;
    return 0;
}

//unwatch the thread
int unwatch(struct chronofs_packet *packet)
{
    // to track if deletion occurred or not
    bool deleted = false;
    for (int i = 0; i < THREAD_ARRAY_TOP; i++)
    {
        //if both paths match
        if(strcmp(watcher_thread_array[i].data->root, packet->path) == 0)
        {
            deleted = true;
            // cancel running thread
            pthread_cancel(watcher_thread_array[i].thread_id);
            //print to its log
            fprintf(watcher_thread_array[i].data->log_file, "Detaching watcher\n");
            //debug print
            printf("Removing watcher thread for dir: %s\n", packet->path);
            //swap deleted one for the last one to keep things compact and dense
            watcher_thread_array[i] = watcher_thread_array[THREAD_ARRAY_TOP - 1];
            watcher_thread_array[THREAD_ARRAY_TOP].active = false;
            THREAD_ARRAY_TOP--;
            break;
        }
    }
    if(!deleted)
    {
        perror("Directory not found to unwatch call\n");
        return -1;
    }
    return 0;
}

// commit the changes and the logs
int commit(struct chronofs_packet *packet)
{
    char chronofs_dir[MAX_PATH_LENGTH];
    strcpy(chronofs_dir, packet->path);
    strcat(chronofs_dir, "/.chronofs");
    DIR *chronofs_dirp = opendir(chronofs_dir);
    if(chronofs_dirp == NULL)
    {
        if(errno == EACCES)
        {
            perror("Access denied\n");
            return -1;
        }
        else if (errno == ENOENT)
        {
            if((mkdir(chronofs_dir, 0755)) == -1)
            {
                printf("Error while trying to make new directory: %s\n", strerror(errno));
                return -1;
            }
            chronofs_dirp = opendir(chronofs_dir);
        }
    }

    char commit_path[MAX_PATH_LENGTH];
    strcpy(commit_path, chronofs_dir);
    time_t now;
    time(&now);
    char now_str[21];
    sprintf(now_str, "%lld", (long long)now);
    strcat(commit_path, "/");
    strcat(commit_path, now_str);
    if(mkdir(commit_path, 0755) == -1)
    {
        perror("Error while trying to make snapshot directory\n");
        return -1;
    }
    if(recursive_copy(packet->path ,commit_path) == -1)
    {
        perror("Error during recursive copying\n");
        return -1;
    }
    closedir(chronofs_dirp);
    return 0;
}

int recursive_copy(char *curr_dir, char *target_dir)
{
    DIR *root_dirp = opendir(curr_dir);
    if(root_dirp == NULL)
    {
        if(errno == EACCES)
            printf("Access Denied\n");
        else if(errno == ENOENT)
            perror("Invalid Path\n");
        return -1;
    }
    struct dirent *dir_entry;
    while((dir_entry = readdir(root_dirp)) != NULL)
    {
        if (strcmp(dir_entry->d_name, ".") != 0 
            && strcmp(dir_entry->d_name, "..") != 0 
            && strcmp(dir_entry->d_name, ".chronofs") != 0
            && dir_entry->d_type == DT_DIR)
        {
            char next_dir[MAX_PATH_LENGTH];
            strcpy(next_dir, curr_dir);
            int last = strlen(next_dir);
            if (last > 0 && next_dir[last - 1] != '/')
            {
                next_dir[last] = '/';
                next_dir[last + 1] = '\0';
            }
            strcat(next_dir, dir_entry->d_name);
            char next_target[MAX_PATH_LENGTH];
            strcpy(next_target, target_dir);
            last = strlen(next_target);
            if (last > 0 && next_target[last - 1] != '/')
            {
                next_target[last] = '/';
                next_target[last + 1] = '\0';
            }
            strcat(next_target, dir_entry->d_name);
            if(mkdir(next_target, 0755) == -1)
            {
                perror("Error while creating target subdirectory");
                return -1;
            }
            if (recursive_copy(next_dir, next_target) == -1)
            {
                printf("Error while recursively copying directory: %s\n", curr_dir);                
            }
        } else if (strcmp(dir_entry->d_name, ".") != 0 
            && strcmp(dir_entry->d_name, "..") != 0 
            && strcmp(dir_entry->d_name, ".chronofs") != 0)
        {
            char src_path[MAX_PATH_LENGTH];
            char dst_path[MAX_PATH_LENGTH];
            strcpy(src_path, curr_dir);
            int last = strlen(src_path);
            if (last > 0 && src_path[last - 1] != '/')
            {
                src_path[last] = '/';
                src_path[last + 1] = '\0';
            }
            strcat(src_path, dir_entry->d_name);
            strcpy(dst_path, target_dir);
            last = strlen(dst_path);
            if(last > 0 && dst_path[last - 1] != '/')
            {
                dst_path[last] = '/';
                dst_path[last + 1] = '\0';
            }
            strcat(dst_path, dir_entry->d_name);
            int src_fd = open(src_path, O_RDONLY);
            int dst_fd = open(
                dst_path,
                O_WRONLY | O_CREAT | O_TRUNC,
                0644
            );
            if(src_fd == -1 || dst_fd == -1)
            {
                perror("File open failure");
                return -1;
            }
            char buffer[MAX_BUFFER_SIZE];
            ssize_t bytes_read;
            while((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0)
            {
                if(write(dst_fd, buffer, bytes_read) == -1)
                {
                    perror("Write failure");
                    close(src_fd);
                    close(dst_fd);
                    return -1;
                }
            }
            if(bytes_read == -1)
            {
                perror("Read failure");
                close(src_fd);
                close(dst_fd);
                return -1;
            }
            close(src_fd);
            close(dst_fd);
        }
    }
    closedir(root_dirp);
    return 0;
}