/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2024  Mike Shal <marfey@gmail.com>
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
#include "tupid_tree.h"
#include "tent_tree.h"
#include "thread_tree.h"
#include "pel_group.h"
#include "entry.h"
#include <stdio.h>
#include <pthread.h>

struct tup_entry;

struct mapping {
	TAILQ_ENTRY(mapping) list;
	char *realname;
	char *tmpname;
	struct tup_entry *tent;
};
TAILQ_HEAD(mapping_head, mapping);

struct tmpdir {
	TAILQ_ENTRY(tmpdir) list;
	char *dirname;
};
TAILQ_HEAD(tmpdir_head, tmpdir);

struct file_entry {
	TAILQ_ENTRY(file_entry) list;
	char *filename;
};
TAILQ_HEAD(file_entry_head, file_entry);

struct file_info {
	pthread_mutex_t lock;
	pthread_cond_t cond;
	struct thread_tree tnode;
	struct file_entry_head read_list;
	struct file_entry_head write_list;
	struct file_entry_head unlink_list;
	struct file_entry_head var_list;
	struct mapping_head mapping_list;
	struct tmpdir_head tmpdir_list;
	struct tent_entries sticky_root;
	struct tent_entries normal_root;
	struct tent_entries group_sticky_root;
	struct tent_entries used_groups_root;
	struct tent_entries output_root;
	struct tent_entries exclusion_root;
	int server_fail;
	int open_count;
	int do_unlink;
};

enum check_type_t {
	CHECK_SUCCESS = 0,
	CHECK_CMDFAIL,
	CHECK_SIGNALLED,
};

int init_file_info(struct file_info *info, int do_unlink);
void cleanup_file_info(struct file_info *info);
void finfo_lock(struct file_info *info);
void finfo_unlock(struct file_info *info);
int handle_file_dtent(enum access_type at, struct tup_entry *dtent,
		      const char *filename, struct file_info *info);
int handle_file(enum access_type at, const char *filename, const char *file2,
		struct file_info *info);
int handle_open_file(enum access_type at, const char *filename,
		     struct file_info *info);
int handle_rename(const char *from, const char *to, struct file_info *info);
int write_files(FILE *f, tupid_t cmdid, struct file_info *info, int *warnings,
		enum check_type_t check_only,
		int full_deps, tupid_t vardt,
		int *important_link_removed);
int add_config_files(struct file_info *finfo, struct tup_entry *tent, int full_deps);
int add_parser_files(struct file_info *finfo, struct tent_entries *root,
		     tupid_t vardt, int full_deps);
void del_map(struct mapping_head *head, struct mapping *map);
void del_file_entry(struct file_entry_head *head, struct file_entry *fent);

#endif
