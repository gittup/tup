#ifndef file_h
#define file_h

#include "tup/tupid.h"

struct access_event;
int handle_file(const struct access_event *event, tupid_t tupid);
int write_files(tupid_t cmdid);

#endif
