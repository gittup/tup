#ifndef file_h
#define file_h

#include "tupid.h"

struct access_event;
int handle_file(const struct access_event *event, const char *filename);
int write_files(tupid_t cmdid, const char *debug_name);

#endif
