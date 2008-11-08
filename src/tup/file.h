#ifndef file_h
#define file_h

#include "tup/tupid.h"

struct access_event;
int handle_file(const struct access_event *event);
int write_files(new_tupid_t cmdid);

#endif
