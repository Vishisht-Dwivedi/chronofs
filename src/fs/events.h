#ifndef EVENTS_H
#define EVENTS_H

#include <time.h>
#include <sys/inotify.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>

#include "../common/protocol.h"

/* Event controller and handlers */
int eventController(struct chronofs_data *data);
int createEvent(struct chronofs_data *data, time_t now, struct inotify_event *event);
int deleteEvent(struct chronofs_data *data, time_t now, struct inotify_event *event);
int modifyEvent(struct chronofs_data *data, time_t now, struct inotify_event *event);
int moveEvent(struct chronofs_data *data, time_t now, struct inotify_event *event);

#endif /* EVENTS_H */