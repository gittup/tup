#ifndef tup_open_notify_h
#define tup_open_notify_h

#include "tup/access_event.h"
struct file_info;

int open_notify_push(struct file_info *finfo);
int open_notify_pop(struct file_info *finfo);
int open_notify(enum access_type at, const char *pathname);

#endif
