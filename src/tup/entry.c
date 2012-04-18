/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2009-2012  Mike Shal <marfey@gmail.com>
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

#define _ATFILE_SOURCE
#include "entry.h"
#include "config.h"
#include "db.h"
#include "compat.h"
#include "colors.h"
#include "container.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

static struct tupid_entries tup_root = RB_INITIALIZER(&tup_root);
static int list_out = 0;
static struct tup_entry_head entry_list;
static int do_verbose = 0;

static struct tup_entry *new_entry(tupid_t tupid, tupid_t dt,
				   const char *name, int len, int type,
				   time_t mtime);
static int tup_entry_add_null(tupid_t tupid, struct tup_entry **dest);
static int rm_entry(tupid_t tupid, int safe);
static int resolve_parent(struct tup_entry *tent);
static int change_name(struct tup_entry *tent, const char *new_name);

int tup_entry_add(tupid_t tupid, struct tup_entry **dest)
{
	struct tup_entry *tent;

	if(tupid <= 0) {
		fprintf(stderr, "tup error: Tupid is %lli in tup_entry_add()\n",
			tupid);
		return -1;
	}

	tent = tup_entry_find(tupid);
	if(tent != NULL) {
		if(dest)
			*dest = tent;
		return 0;
	}

	tent = new_entry(tupid, -1, NULL, 0, -1, -1);
	if(!tent)
		return -1;

	if(tup_db_fill_tup_entry(tupid, tent) < 0)
		return -1;

	if(tup_entry_add_null(tent->dt, &tent->parent) < 0)
		return -1;

	if(tent->parent) {
		if(string_tree_insert(&tent->parent->entries, &tent->name) < 0) {
			fprintf(stderr, "tup error: Unable to insert node named '%s' into parent's (id=%lli) string tree.\n", tent->name.s, tent->parent->tnode.tupid);
			return -1;
		}
	}
	if(dest)
		*dest = tent;
	return 0;
}

int tup_entry_find_name_in_dir(tupid_t dt, const char *name, int len,
			       struct tup_entry **dest)
{
	struct tup_entry *parent;
	struct string_tree *st;

	if(len < 0)
		len = strlen(name);

	if(dt == 0) {
		if(strncmp(name, ".", len) == 0) {
			*dest = tup_entry_find(DOT_DT);
			return 0;
		}
		fprintf(stderr, "tup error: entry '%.*s' shouldn't have dt == 0\n", len, name);
		return -1;
	}

	if(tup_entry_add(dt, &parent) < 0) {
		fprintf(stderr, "tup error: Unable to find parent entry [%lli] for node '%.*s'\n", dt, len, name);
		return -1;
	}
	st = string_tree_search(&parent->entries, name, len);
	if(!st) {
		*dest = NULL;
		return 0;
	}
	*dest = container_of(st, struct tup_entry, name);
	return 0;
}

int tup_entry_rm(tupid_t tupid)
{
	return rm_entry(tupid, 0);
}

static int rm_entry(tupid_t tupid, int safe)
{
	struct tup_entry *tent;

	tent = tup_entry_find(tupid);
	if(!tent) {
		/* Some nodes may be removed from the database without ever
		 * being cached - for example, when a command is no longer
		 * created by a Tupfile we don't need to read in the entry (it
		 * is all handled by tupid).
		 */
		return 0;
	}
	if(!RB_EMPTY(&tent->entries)) {
		if(safe) {
			return 0;
		} else {
			fprintf(stderr, "tup internal error: tup_entry_rm called on tupid %lli, which still has entries\n", tupid);
			return -1;
		}
	}

	if(tent->list.le_prev != NULL) {
		fprintf(stderr, "tup internal error: tup_entry_rm called on tupid %lli, which is in the entry list [%lli:%s]\n", tupid, tent->dt, tent->name.s);
		return -1;
	}
	if(tent->ghost_list.le_prev != NULL) {
		LIST_REMOVE(tent, ghost_list);
	}

	tupid_tree_rm(&tup_root, &tent->tnode);
	if(tent->parent) {
		string_tree_rm(&tent->parent->entries, &tent->name);
	}
	free(tent->name.s);
	free(tent);
	return 0;
}

struct tup_entry *tup_entry_get(tupid_t tupid)
{
	struct tup_entry *tent;

	tent = tup_entry_find(tupid);
	if(!tent) {
		fprintf(stderr, "tup internal error: Unable to find tup entry %lli in tup_entry_get()\n", tupid);
		exit(1);
	}
	return tent;
}

struct tup_entry *tup_entry_find(tupid_t tupid)
{
	struct tupid_tree *tnode;
	tnode = tupid_tree_search(&tup_root, tupid);
	if(!tnode)
		return NULL;
	return container_of(tnode, struct tup_entry, tnode);
}

void tup_entry_set_verbose(int verbose)
{
	do_verbose = verbose;
}

/* Returns 0 in case if a root tup entry has been passed and thus nothing has
 * been printed, otherwise 1 is returned.
 */
static int print_tup_entry_internal(FILE *f, struct tup_entry *tent)
{
	/* Skip empty entries, and skip '.' here (tent->parent == NULL) */
	if(!tent || !tent->parent)
		return 0;
	if(print_tup_entry_internal(f, tent->parent))
		fprintf(f, "%s", PATH_SEP_STR);
	/* Don't print anything for the slash root entry */
	if(tent->name.s[0] != '/')
		fprintf(f, "%s", tent->name.s);
	return 1;
}

void print_tup_entry(FILE *f, struct tup_entry *tent)
{
	const char *name;
	int name_sz = 0;

	if(!tent)
		return;
	if(print_tup_entry_internal(f, tent->parent)) {
		const char *sep = tent->type == TUP_NODE_CMD ? ": " : PATH_SEP_STR;
		fprintf(f, "%s", sep);
	}
	name = tent->name.s;
	name_sz = tent->name.len;
	if(!do_verbose && name[0] == '^') {
		name++;
		while(*name && *name != ' ' && *name != '^') name++;
		if(*name == '^') {
			/* If we just have ^-flags but no TEXT, then print the rest of the
			 * string verbatim.
			 */
			name++;
			while(isspace(*name)) name++;
			name_sz = strlen(name);
		} else {
			/* If we have ^ TEXT^, then just capture the TEXT part */
			name++;
			name_sz = 0;
			while(name[name_sz] && name[name_sz] != '^')
				name_sz++;
		}
	}

	color_set(f);
	fprintf(f, "%s%s%.*s%s", color_type(tent->type), color_append_normal(), name_sz, name, color_end());
}

int snprint_tup_entry(char *dest, int len, struct tup_entry *tent)
{
	int rc;
	if(!tent || !tent->parent)
		return 0;
	rc = snprint_tup_entry(dest, len, tent->parent);
	rc += snprintf(dest + rc, len - rc, "/%s", tent->name.s);
	return rc;
}

static int tup_entry_add_null(tupid_t tupid, struct tup_entry **dest)
{
	if(tupid == 0) {
		if(dest)
			*dest = NULL;
		return 0;
	}
	return tup_entry_add(tupid, dest);
}

int tup_entry_add_to_dir(tupid_t dt, tupid_t tupid, const char *name, int len,
			 int type, time_t mtime,
			 struct tup_entry **dest)
{
	struct tup_entry *tent;

	tent = new_entry(tupid, dt, name, len, type, mtime);
	if(!tent)
		return -1;
	if(resolve_parent(tent) < 0)
		return -1;
	if(dest)
		*dest = tent;
	return 0;
}

int tup_entry_add_all(tupid_t tupid, tupid_t dt, int type,
		      time_t mtime, const char *name, struct tupid_entries *root)
{
	struct tup_entry *tent;

	tent = new_entry(tupid, dt, name, strlen(name), type, mtime);
	if(!tent)
		return -1;

	if(root)
		if(tupid_tree_add(root, tupid) < 0)
			return -1;
	return 0;
}

int tup_entry_resolve_dirs(void)
{
	struct tupid_tree *tt;
	/* TODO: NEeded? */
	/* Resolve parents - those will all already be loaded into the tree.
	 */
	RB_FOREACH(tt, tupid_entries, &tup_root) {
		struct tup_entry *tent = container_of(tt, struct tup_entry, tnode);

		if(resolve_parent(tent) < 0)
			return -1;
	}
	return 0;
}

int tup_entry_open_tupid(tupid_t tupid)
{
	struct tup_entry *tent;

	if(tup_entry_add(tupid, &tent) < 0)
		return -1;
	return tup_entry_open(tent);
}

int tup_entry_open(struct tup_entry *tent)
{
	return tup_entry_openat(tup_top_fd(), tent);
}

int tup_entry_openat(int root_dfd, struct tup_entry *tent)
{
	int dfd;
	int newdfd;

	if(!tent)
		return -1;
	if(tent->parent == NULL) {
		return dup(root_dfd);
	}

	dfd = tup_entry_openat(root_dfd, tent->parent);
	if(dfd < 0)
		return dfd;

	newdfd = openat(dfd, tent->name.s, O_RDONLY);
	if(close(dfd) < 0) {
		perror("close(dfd)");
		return -1;
	}
	if(newdfd < 0) {
		if(errno == ENOENT)
			return -ENOENT;
		perror(tent->name.s);
		return -1;
	}
	return newdfd;
}

static struct tup_entry *new_entry(tupid_t tupid, tupid_t dt,
				   const char *name, int len, int type,
				   time_t mtime)
{
	struct tup_entry *tent;

	tent = malloc(sizeof *tent);
	if(!tent) {
		perror("malloc");
		return NULL;
	}

	if(len == -1)
		len = strlen(name);

	tent->tnode.tupid = tupid;
	tent->list.le_prev = NULL;
	tent->ghost_list.le_prev = NULL;
	tent->dt = dt;
	tent->parent = NULL;
	tent->type = type;
	tent->mtime = mtime;
	if(name) {
		tent->name.s = malloc(len+1);
		if(!tent->name.s) {
			perror("malloc");
			free(tent);
			return NULL;
		}
		strncpy(tent->name.s, name, len);
		tent->name.s[len] = 0;
		tent->name.len = len;
	} else {
		tent->name.s = NULL;
		tent->name.len = 0;
	}
	RB_INIT(&tent->entries);

	if(tupid_tree_insert(&tup_root, &tent->tnode) < 0) {
		fprintf(stderr, "tup error: Unable to insert node %lli into the tupid tree in new_entry\n", tent->tnode.tupid);
		tup_db_print(stderr, tent->tnode.tupid);
		return NULL;
	}

	return tent;
}

static int resolve_parent(struct tup_entry *tent)
{
	if(tent->dt == 0) {
		tent->parent = NULL;
	} else {
		tent->parent = tup_entry_find(tent->dt);
		if(!tent->parent) {
			fprintf(stderr, "tup error: Unable to find parent entry [%lli] for node %lli.\n", tent->dt, tent->tnode.tupid);
			return -1;
		}
		if(string_tree_insert(&tent->parent->entries, &tent->name) < 0) {
			fprintf(stderr, "tup error: Unable to insert node named '%s' into parent's (id=%lli) string tree.\n", tent->name.s, tent->parent->tnode.tupid);
			return -1;
		}
	}
	return 0;
}

int tup_entry_change_name(tupid_t tupid, const char *new_name)
{
	struct tup_entry *tent;

	tent = tup_entry_get(tupid);
	return change_name(tent, new_name);
}

int tup_entry_change_name_dt(tupid_t tupid, const char *new_name,
			     tupid_t new_dt)
{
	struct tup_entry *tent;

	tent = tup_entry_get(tupid);
	tent->dt = new_dt;

	return change_name(tent, new_name);
}

int tup_entry_clear(void)
{
	/* The rm_entry with safe=1 will only remove the node if all of the
	 * children nodes are gone. Rather than try to smartly remove things
	 * in the correct order, the outer loop will just keep going until
	 * all the nodes are gone, and the inner loop does a single pass over
	 * the whole tree. Eventually everything will be gone.
	 *
	 * I don't really care about performance here, since this only happens
	 * when the monitor needs to restart or during certain database
	 * upgrades.
	 */
	while(!RB_EMPTY(&tup_root)) {
		struct tupid_tree *tt;
		struct tupid_tree *tmp;

		RB_FOREACH_SAFE(tt, tupid_entries, &tup_root, tmp) {
			if(rm_entry(tt->tupid, 1) < 0)
				return -1;
		}
	}
	return 0;
}

struct tup_entry_head *tup_entry_get_list(void)
{
	if(list_out) {
		fprintf(stderr, "tup internal error: entry list is already out\n");
		exit(1);
	}
	list_out = 1;
	LIST_INIT(&entry_list);
	return &entry_list;
}

void tup_entry_release_list(void)
{
	if(!list_out) {
		fprintf(stderr, "tup internal error: entry list isn't out\n");
		exit(1);
	}
	while(!LIST_EMPTY(&entry_list)) {
		struct tup_entry *tent = LIST_FIRST(&entry_list);
		tup_entry_list_del(tent);
	}
	list_out = 0;
}

void tup_entry_list_add(struct tup_entry *tent, struct tup_entry_head *head)
{
	if(!list_out) {
		fprintf(stderr, "tup internal error: tup_entry_list_add called without the list\n");
		exit(1);
	}
	if(tent->list.le_prev == NULL) {
		LIST_INSERT_HEAD(head, tent, list);
	}
}

void tup_entry_list_del(struct tup_entry *tent)
{
	if(!list_out) {
		fprintf(stderr, "tup internal error: tup_entry_list_del called without the list\n");
		exit(1);
	}
	LIST_REMOVE(tent, list);
	tent->list.le_prev = NULL;
}

int tup_entry_in_list(struct tup_entry *tent)
{
	return !(tent->list.le_prev == NULL);
}

void tup_entry_add_ghost_list(struct tup_entry *tent, struct tup_entry_head *head)
{
	if(tent->type == TUP_NODE_GHOST) {
		/* It is fine if the ghost is already in the list - just make
		 * sure we don't try to add it twice.
		 */
		if(tent->ghost_list.le_prev == NULL) {
			LIST_INSERT_HEAD(head, tent, ghost_list);
		}
	}
}

int tup_entry_del_ghost_list(struct tup_entry *tent)
{
	if(tent->ghost_list.le_prev == NULL) {
		fprintf(stderr, "tup internal error: ghost_list.next is NULL in tup_entry_del_ghost_list %lli [%lli:%s]\n", tent->tnode.tupid, tent->dt, tent->name.s);
		return -1;
	}
	LIST_REMOVE(tent, ghost_list);
	tent->ghost_list.le_prev = NULL;
	return 0;
}

int tup_entry_debug_add_all_ghosts(struct tup_entry_head *head)
{
	struct tupid_tree *tt;

	RB_FOREACH(tt, tupid_entries, &tup_root) {
		struct tup_entry *tent;

		tent = container_of(tt, struct tup_entry, tnode);
		tup_entry_add_ghost_list(tent, head);
	}
	return 0;
}

static int change_name(struct tup_entry *tent, const char *new_name)
{
	if(tent->parent) {
		string_tree_rm(&tent->parent->entries, &tent->name);
	}
	free(tent->name.s);

	tent->name.len = strlen(new_name);
	tent->name.s = malloc(tent->name.len + 1);
	if(!tent->name.s) {
		perror("malloc");
		return -1;
	}
	strcpy(tent->name.s, new_name);
	if(resolve_parent(tent) < 0)
		return -1;
	return 0;
}

void dump_tup_entry(void)
{
	struct tupid_tree *tt;

	printf("Tup entries:\n");
	RB_FOREACH(tt, tupid_entries, &tup_root) {
		struct tup_entry *tent;

		tent = container_of(tt, struct tup_entry, tnode);
		printf("  [%lli, dir=%lli, type=%i] name=%s\n", tent->tnode.tupid, tent->dt, tent->type, tent->name.s);
	}
}

// todo - better name
int get_tent_list(tupid_t tupid, struct tent_list_head *head)
{
	struct tup_entry *tent;
	struct tent_list *tlist;

	tent = tup_entry_get(tupid);
	while(tent) {
		tlist = malloc(sizeof *tlist);
		if(!tlist) {
			perror("malloc");
			return -1;
		}
		tlist->tent = tent;
		TAILQ_INSERT_HEAD(head, tlist, list);

		tent = tent->parent;
	}
	return 0;
}

void del_tent_list_entry(struct tent_list_head *head, struct tent_list *tlist)
{
	TAILQ_REMOVE(head, tlist, list);
	free(tlist);
}

void free_tent_list(struct tent_list_head *head)
{
	struct tent_list *tlist;
	while(!TAILQ_EMPTY(head)) {
		tlist = TAILQ_FIRST(head);
		del_tent_list_entry(head, tlist);
	}
}

int get_relative_dir(char *dest, tupid_t start, tupid_t end, int *len)
{
	struct tent_list_head startlist;
	struct tent_list_head endlist;
	struct tent_list *startentry;
	struct tent_list *endentry;
	int first = 0;

	*len = 0;

	TAILQ_INIT(&startlist);
	TAILQ_INIT(&endlist);
	if(get_tent_list(start, &startlist) < 0)
		return -1;
	if(get_tent_list(end, &endlist) < 0)
		return -1;

	while(!TAILQ_EMPTY(&startlist) && !TAILQ_EMPTY(&endlist)) {
		startentry = TAILQ_FIRST(&startlist);
		endentry = TAILQ_FIRST(&endlist);

		if(startentry->tent == endentry->tent) {
			del_tent_list_entry(&startlist, startentry);
			del_tent_list_entry(&endlist, endentry);
		} else {
			break;
		}
	}

	TAILQ_FOREACH(startentry, &startlist, list) {
		if(!first) {
			first = 1;
		} else {
			if(dest)
				sprintf(dest + *len, "/");
			(*len)++;
		}
		if(dest)
			sprintf(dest + *len, "..");
		(*len) += 2;
	}
	TAILQ_FOREACH(endentry, &endlist, list) {
		if(!first) {
			first = 1;
		} else {
			if(dest)
				sprintf(dest + *len, "/");
			(*len)++;
		}
		if(dest)
			sprintf(dest + *len, "%s", endentry->tent->name.s);
		(*len) += endentry->tent->name.len;
	}
	if(!first) {
		if(dest)
			sprintf(dest + *len, ".");
		(*len)++;
	}

	free_tent_list(&endlist);
	free_tent_list(&startlist);
	return 0;
}

