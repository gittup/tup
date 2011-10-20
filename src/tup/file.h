#ifndef file_h
#define file_h

#include "tupid.h"
#include "access_event.h"
#include "bsd/queue.h"
#include "thread_tree.h"
#include "pel_group.h"
#include <pthread.h>

struct tup_entry;
struct tupid_entries;

struct mapping {
	LIST_ENTRY(mapping) list;
	char *realname;
	char *tmpname;
	struct tup_entry *tent;
};
LIST_HEAD(mapping_head, mapping);

struct tmpdir {
	LIST_ENTRY(tmpdir) list;
	char *dirname;
};
LIST_HEAD(tmpdir_head, tmpdir);

struct file_entry {
	LIST_ENTRY(file_entry) list;
	tupid_t dt;
	char *filename;
	struct pel_group pg;
};
LIST_HEAD(file_entry_head, file_entry);

struct file_info {
	pthread_mutex_t lock;
	struct thread_tree tnode;
	struct file_entry_head read_list;
	struct file_entry_head write_list;
	struct file_entry_head unlink_list;
	struct file_entry_head var_list;
	struct mapping_head mapping_list;
	struct tmpdir_head tmpdir_list;
	int server_fail;
};

int init_file_info(struct file_info *info);
void finfo_lock(struct file_info *info);
void finfo_unlock(struct file_info *info);
int handle_file(enum access_type at, const char *filename, const char *file2,
		struct file_info *info, tupid_t dt);
int handle_open_file(enum access_type at, const char *filename,
		     struct file_info *info, tupid_t dt);
int handle_rename(const char *from, const char *to, struct file_info *info);
int write_files(tupid_t cmdid, struct file_info *info, int *warnings,
		int check_only);
int add_parser_files(struct file_info *info, struct tupid_entries *root);
void del_map(struct mapping *map);

#endif
