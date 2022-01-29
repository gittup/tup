/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2009-2022  Mike Shal <marfey@gmail.com>
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

#ifndef entry_h
#define entry_h

#include "tupid_tree.h"
#include "tent_tree.h"
#include "string_tree.h"
#include "db_types.h"
#include "bsd/queue.h"
#include <stdio.h>
#include <time.h>
#include <pcre.h>

struct variant;
struct estring;

/* Local cache of the entries in the 'node' database table */
struct tup_entry {
	struct tupid_tree tnode;
	tupid_t dt;
	struct tup_entry *parent;
	enum TUP_NODE_TYPE type;
	struct timespec mtime;
	tupid_t srcid;
	struct variant *variant;
	struct string_tree name;
	struct string_entries entries;
	struct tent_entries stickies;
	struct tent_entries group_stickies;
	int retrieved_stickies;
	struct tup_entry *incoming;
	_Atomic int refcount;

	/* For exclusions */
	pcre *re;

	/* For command strings */
	char *flags;
	int flagslen;
	char *display;
	int displaylen;
};

LIST_HEAD(tup_entry_head, tup_entry);

int tup_entry_add(tupid_t tupid, struct tup_entry **dest);
int tup_entry_find_name_in_dir(struct tup_entry *tent, const char *name, int len,
			       struct tup_entry **dest);
int tup_entry_find_name_in_dir_dt(struct tup_entry *dtent, const char *name, int len,
				  struct tup_entry **dest);
int tup_entry_add_to_dir(struct tup_entry *dtent, tupid_t tupid, const char *name, int len,
			 const char *display, int displaylen, const char *flags, int flagslen,
			 enum TUP_NODE_TYPE type, struct timespec mtime, tupid_t srcid,
			 struct tup_entry **dest);
int tup_entry_add_all(tupid_t tupid, tupid_t dt, enum TUP_NODE_TYPE type,
		      struct timespec mtime, tupid_t srcid, const char *name, const char *display, const char *flags,
		      struct tup_entry **dest);
int tup_entry_resolve_dirs(void);
int tup_entry_change_name_dt(tupid_t tupid, const char *new_name, tupid_t dt);
int tup_entry_change_display(struct tup_entry *tent, const char *display, int displaylen);
int tup_entry_change_flags(struct tup_entry *tent, const char *flags, int flagslen);
int tup_entry_open(struct tup_entry *tent);
int tup_entry_openat(int root_dfd, struct tup_entry *tent);
void tup_entry_add_ref(struct tup_entry *tent);
void tup_entry_del_ref(struct tup_entry *tent);
struct variant *tup_entry_variant(struct tup_entry *tent);
struct variant *tup_entry_variant_null(struct tup_entry *tent);
tupid_t tup_entry_vardt(struct tup_entry *tent);
int tup_entry_rm(tupid_t tupid);
struct tup_entry *tup_entry_get(tupid_t tupid);
struct tup_entry *tup_entry_find(tupid_t tupid);
int tup_entry_sym_follow(struct tup_entry *tent);
void tup_entry_set_verbose(int verbose);
void print_tup_entry(FILE *f, struct tup_entry *tent);
void print_tupid(FILE *f, tupid_t tupid);
int snprint_tup_entry(char *dest, int len, struct tup_entry *tent);
int write_tup_entry(FILE *f, struct tup_entry *tent);
int tup_entry_clear(void);
int tup_entry_add_ghost_tree(struct tent_entries *root, struct tup_entry *tent);
int tup_entry_debug_add_all_ghosts(struct tent_entries *root);
int tup_entry_get_dir_tree(struct tup_entry *tent, struct tupid_entries *root);
void dump_tup_entry(void);
int get_relative_dir(FILE *f, struct estring *e, tupid_t start, tupid_t end);
int get_relative_dir_sep(FILE *f, struct estring *e, tupid_t start, tupid_t end, char sep);
int is_transient_tent(struct tup_entry *tent);
int is_compiledb_tent(struct tup_entry *tent);
int exclusion_match(FILE *f, struct tent_entries *exclusion_root, const char *s, int *match);

#endif
