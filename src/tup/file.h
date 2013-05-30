/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2013  Mike Shal <marfey@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef file_h
#define file_h

#include "tupid.h"
#include "access_event.h"
#include "bsd/queue.h"
#include "thread_tree.h"
#include "pel_group.h"
#include <stdio.h>
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
	const char *variant_dir;
	int server_fail;
};

int init_file_info(struct file_info *info, const char *variant_dir);
void finfo_lock(struct file_info *info);
void finfo_unlock(struct file_info *info);
int handle_file(enum access_type at, const char *filename, const char *file2,
		struct file_info *info);
int handle_open_file(enum access_type at, const char *filename,
		     struct file_info *info);
int handle_rename(const char *from, const char *to, struct file_info *info);
int write_files(FILE *f, tupid_t cmdid, struct file_info *info, int *warnings,
		int check_only, struct tupid_entries *sticky_root,
		struct tupid_entries *normal_root,
		struct tupid_entries *group_sticky_root,
		int full_deps, tupid_t vardt, int used_groups);
int add_config_files(struct file_info *finfo, struct tup_entry *tent);
int add_parser_files(FILE *f, struct file_info *finfo, struct tupid_entries *root, tupid_t vardt);
void del_map(struct mapping *map);

#endif
