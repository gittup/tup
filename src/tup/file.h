#ifndef file_h
#define file_h

#include "tupid.h"
#include "access_event.h"
#include "linux/list.h"
#include "thread_tree.h"

struct mapping {
	struct list_head list;
	char *realname;
	char *tmpname;
};

struct tmpdir {
	struct list_head list;
	char *dirname;
};

struct file_info {
	struct thread_tree tnode;
	struct list_head read_list;
	struct list_head write_list;
	struct list_head unlink_list;
	struct list_head var_list;
	struct list_head sym_list;
	struct list_head mapping_list;
	struct list_head tmpdir_list;
};

struct tup_entry;

int init_file_info(struct file_info *info);
int handle_file(enum access_type at, const char *filename, const char *file2,
		struct file_info *info, tupid_t dt);
int handle_open_file(enum access_type at, const char *filename,
		     struct file_info *info, tupid_t dt);
int handle_rename(const char *from, const char *to, struct file_info *info);
int write_files(tupid_t cmdid, const char *debug_name, struct file_info *info,
		int *warnings);
int add_parser_files(struct file_info *info, struct rb_root *root);
void del_map(struct mapping *map);

#endif
