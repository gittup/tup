#ifndef file_h
#define file_h

#include "tupid.h"
#include "list.h"

struct file_info {
	struct list_head read_list;
	struct list_head write_list;
	struct list_head rename_list;
	struct list_head unlink_list;
};

struct access_event;
int init_file_info(struct file_info *info);
int handle_file(const struct access_event *event, const char *filename,
		struct file_info *info);
int write_files(tupid_t cmdid, const char *debug_name, struct file_info *info);

#endif
