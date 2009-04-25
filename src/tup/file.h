#ifndef file_h
#define file_h

#include "tupid.h"
#include "list.h"

struct file_info {
	struct list_head read_list;
	struct list_head write_list;
	struct list_head unlink_list;
	struct list_head var_list;
	struct list_head sym_list;
};

enum access_type;
int init_file_info(struct file_info *info);
int handle_file(enum access_type at, const char *filename, const char *file2,
		struct file_info *info);
int write_files(tupid_t cmdid, tupid_t old_cmdid, tupid_t dt,
		const char *debug_name, struct file_info *info);

#endif
