#ifndef file_h
#define file_h

#include "tupid.h"
#include "access_event.h"
#include "linux/list.h"

struct file_info {
	struct list_head read_list;
	struct list_head write_list;
	struct list_head unlink_list;
	struct list_head var_list;
	struct list_head sym_list;
	struct list_head ghost_list;
};

struct tup_entry;

int init_file_info(struct file_info *info);
int handle_file(enum access_type at, const char *filename, const char *file2,
		struct file_info *info, tupid_t dt);
int write_files(tupid_t cmdid, const char *debug_name, struct file_info *info,
		int *warnings);
int file_set_mtime(struct tup_entry *tent, int dfd, const char *file);

#endif
