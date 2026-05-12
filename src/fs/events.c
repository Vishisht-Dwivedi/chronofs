#include "../common/utilities.h"
#include "events.h"

//event controller
int eventController(struct chronofs_data *data) {
    //buffer where fd will be read and stored
    char buffer[4096];
    time_t now;
    time(&now);
    // infinite loop
    while (true)
    {
        //size of file descriptor buffer received from inotify_init
        ssize_t len = read(data->inotify_fd, buffer, sizeof(buffer));
        if(len < 0)
        {
            perror("Error during reading the file descriptor\n");
            return -1;
        }
        //reading all changes
        for (int i = 0; i < len;)
        {
            //cast buffer to event pointer
            struct inotify_event *event = (struct inotify_event *)&buffer[i];
            //skip log file changes
            if(event->len>0 && strcmp(event->name, "current.log") != 0){
                time(&now);
                //if creation event
                if (IN_CREATE & event->mask)
                    if(createEvent(data, now, event) == -1)
                        return -1;
                // handle deletes
                if (IN_DELETE & event->mask)
                    if(deleteEvent(data, now, event) == -1)
                        return -1;
                if (IN_CLOSE_WRITE & event->mask)
                    if(modifyEvent(data, now, event) == -1)
                        return -1;
                if ((event->mask & IN_MOVED_FROM) || (event->mask & IN_MOVED_TO))
                    if(moveEvent(data, now, event) == -1)
                        return -1;
            }
            //event struct + variable length of name
            i += sizeof(struct inotify_event) + event->len;
        }
    }
    return 0;
}

//event handlers

int createEvent(struct chronofs_data *data, time_t now, struct inotify_event *event)
{
    fprintf(data->log_file,"%ld Created %s\n", now, event->name);
    fflush(data->log_file);
    //if directory is created
    if(event->mask & IN_ISDIR)
    {
        //find parent wd
        for (int j = 0; j < data->wd_top; j++)
        {
            if(data->wd_map[j].wd == event->wd)
            {
                char new_dir_path[MAX_PATH_LENGTH];
                strcpy(new_dir_path, data->wd_map[j].path);
                int last = strlen(new_dir_path);
                if (last > 0 && new_dir_path[last - 1] != '/') {
                    new_dir_path[last] = '/';
                    new_dir_path[last + 1] = '\0';
                }
                //add generated path name to parent path
                strcat(new_dir_path, event->name);
                int new_wd = inotify_add_watch(data->inotify_fd, new_dir_path, IN_ALL_EVENTS);
                if (new_wd == -1)
                {
                    perror("Dynamic directory watch event listener failure\n");
                    return -1;
                }
                struct wd_map new_dir;
                new_dir.wd = new_wd;
                new_dir.active = true;
                strcpy(new_dir.path, new_dir_path);
                // resize if needed
                if(data->wd_top == data->wd_arr_size)
                {
                    if(wd_map_extend(data) == -1)
                    {
                        perror("Error while allocating new memory to map within dynamic wd allocation\n");
                        return -1;
                    }
                }
                data->wd_map[data->wd_top++] = new_dir;
                break;
            }
        }
    }
    return 0;
}
int deleteEvent(struct chronofs_data *data, time_t now, struct inotify_event *event) 
{
    fprintf(data->log_file ,"%ld Deleted %s\n", now, event->name);
    fflush(data->log_file);
    //if directory delete
    if(event->mask & IN_ISDIR){
        //find parent wd
        for (int j = 0; j < data->wd_top; j++)
        {
            if (data->wd_map[j].wd == event->wd)
            {
                //get parent path
                char deleted_path[MAX_PATH_LENGTH];
                strcpy(deleted_path, data->wd_map[j].path);
                int last = strlen(deleted_path);
                if(last>0 && deleted_path[last-1]!='/')
                {
                    deleted_path[last] = '/';
                    deleted_path[last + 1] = '\0';
                }
                //get deleted child's path
                strcat(deleted_path, event->name);
                //find child descriptor and remove it from watch list
                for (int k = 0; k < data->wd_top; k++)
                {
                    if(strcmp(data->wd_map[k].path, deleted_path) == 0)
                    {
                        inotify_rm_watch(data->inotify_fd, data->wd_map[k].wd);
                        data->wd_map[k].active = false;
                        data->wd_map[k] = data->wd_map[data->wd_top - 1];
                        data->wd_top--;
                        break;
                    }
                }
                break;
            }
        }
    }
    return 0;
}
int modifyEvent(struct chronofs_data *data, time_t now, struct inotify_event *event) {
    fprintf(data->log_file, "%ld Modified %s\n", now, event->name);
    fflush(data->log_file);
    return 0;
}
int moveEvent(struct chronofs_data *data, time_t now, struct inotify_event *event) {
    if (event->mask & IN_MOVED_FROM)
    {
        fprintf(data->log_file, "%ld Moved from %s\n", now, event->name);
        fflush(data->log_file);
    }
    if (event->mask & IN_MOVED_TO)
    {
        fprintf(data->log_file, "%ld Moved to %s\n", now, event->name);
        fflush(data->log_file);
    }
    return 0;
}
