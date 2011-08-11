#define _ATFILE_SOURCE
#include "entry.h"
#include "config.h"
#include "db.h"
#include "compat.h"
#include "colors.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

static struct rb_root tup_tree = RB_ROOT;
static int list_out = 0;
static struct list_head entry_list;

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
		fprintf(stderr, "Error: Tupid is %lli in tup_entry_add()\n",
			tupid);
		return -1;
	}

	tent = tup_entry_find(tupid);
	if(tent != NULL) {
		if(dest)
			*dest = tent;
		return 0;
	}

	tent = malloc(sizeof *tent);
	if(!tent) {
		perror("malloc");
		return -1;
	}
	tent->tnode.tupid = tupid;
	tent->list.next = NULL;
	tent->ghost_list.next = NULL;
	tent->entries.rb_node = NULL;
	tent->parent = NULL;

	if(tup_db_fill_tup_entry(tupid, tent) < 0)
		return -1;

	if(tupid_tree_insert(&tup_tree, &tent->tnode) < 0) {
		fprintf(stderr, "tup error: Unable to insert node %lli into the tupid tree\n", tent->tnode.tupid);
		return -1;
	}

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

	parent = tup_entry_find(dt);
	if(!parent) {
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
	if(tent->entries.rb_node != NULL) {
		if(safe) {
			return 0;
		} else {
			fprintf(stderr, "tup internal error: tup_entry_rm called on tupid %lli, which still has entries\n", tupid);
			return -1;
		}
	}

	if(tent->list.next != NULL) {
		fprintf(stderr, "tup internal error: tup_entry_rm called on tupid %lli, which is in the entry list [%lli:%s]\n", tupid, tent->dt, tent->name.s);
		return -1;
	}
	if(tent->ghost_list.next != NULL) {
		list_del(&tent->ghost_list);
	}

	tupid_tree_rm(&tup_tree, &tent->tnode);
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
	tnode = tupid_tree_search(&tup_tree, tupid);
	if(!tnode)
		return NULL;
	return container_of(tnode, struct tup_entry, tnode);
}

/*
 * Returns 0 in case if a root tup entry has been passed and thus nothing has been printed,
 * otherwise 1 is returned.
 */
static int __print_tup_entry(FILE *f, struct tup_entry *tent)
{
	/* Skip empty entries, and skip '.' here (tent->parent == NULL) */
	if(!tent || !tent->parent)
		return 0;
	if (__print_tup_entry(f, tent->parent))
		fprintf(f, "%s", PATH_SEP_STR);
	fprintf(f, "%s", tent->name.s);
	return 1;
}

void print_tup_entry(FILE *f, struct tup_entry *tent)
{
	const char *name;
	int name_sz = 0;

	if(!tent)
		return;
	if (__print_tup_entry(f, tent->parent)) {
		const char *sep = tent->type == TUP_NODE_CMD ? ": " : PATH_SEP_STR;
		fprintf(f, "%s", sep);
	}
	name = tent->name.s;
	name_sz = tent->name.len;
	if(name[0] == '^') {
		name++;
		while(*name && *name != ' ') name++;
		name++;
		name_sz = 0;
		while(name[name_sz] && name[name_sz] != '^')
			name_sz++;
	}

	fprintf(f, "%s%s%.*s%s", color_type(tent->type), color_append_normal(), name_sz, name, color_end());
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
		      time_t mtime, const char *name, struct rb_root *tree)
{
	struct tup_entry *tent;

	tent = new_entry(tupid, dt, name, strlen(name), type, mtime);
	if(!tent)
		return -1;

	if(tree)
		if(tupid_tree_add(tree, tupid) < 0)
			return -1;
	return 0;
}

int tup_entry_resolve_dirs(void)
{
	struct rb_node *rbn;

	/* TODO: NEeded? */
	/* Resolve parents - those will all already be loaded into the tree.
	 */
	for(rbn = rb_first(&tup_tree); rbn; rbn = rb_next(rbn)) {
		struct tupid_tree *tnode = rb_entry(rbn, struct tupid_tree, rbn);
		struct tup_entry *tent = container_of(tnode, struct tup_entry, tnode);

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
	close(dfd);
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
	tent->list.next = NULL;
	tent->ghost_list.next = NULL;
	tent->dt = dt;
	tent->parent = NULL;
	tent->type = type;
	tent->mtime = mtime;
	tent->name.s = malloc(len+1);
	if(!tent->name.s) {
		perror("malloc");
		free(tent);
		return NULL;
	}
	strncpy(tent->name.s, name, len);
	tent->name.s[len] = 0;
	tent->name.len = len;
	tent->entries.rb_node = NULL;

	if(tupid_tree_insert(&tup_tree, &tent->tnode) < 0) {
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
	struct rb_node *rbn;

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
	while((rbn = rb_first(&tup_tree)) != NULL) {
		struct rb_node *next;
		struct tupid_tree *tt;

		while(rbn != NULL) {
			next = rb_next(rbn);
			tt = rb_entry(rbn, struct tupid_tree, rbn);
			if(rm_entry(tt->tupid, 1) < 0)
				return -1;
			rbn = next;
		}
	}
	return 0;
}

struct list_head *tup_entry_get_list(void)
{
	if(list_out) {
		fprintf(stderr, "tup internal error: entry list is already out\n");
		exit(1);
	}
	list_out = 1;
	INIT_LIST_HEAD(&entry_list);
	return &entry_list;
}

void tup_entry_release_list(void)
{
	if(!list_out) {
		fprintf(stderr, "tup internal error: entry list isn't out\n");
		exit(1);
	}
	while(!list_empty(&entry_list)) {
		struct tup_entry *tent;
		tent = list_entry(entry_list.next, struct tup_entry, list);
		tup_entry_list_del(tent);
	}
	list_out = 0;
}

void tup_entry_list_add(struct tup_entry *tent, struct list_head *list)
{
	if(!list_out) {
		fprintf(stderr, "tup internal error: tup_entry_list_add called without the list\n");
		exit(1);
	}
	if(tent->list.next == NULL) {
		list_add(&tent->list, list);
	}
}

void tup_entry_list_del(struct tup_entry *tent)
{
	if(!list_out) {
		fprintf(stderr, "tup internal error: tup_entry_list_del called without the list\n");
		exit(1);
	}
	list_del(&tent->list);
	tent->list.next = NULL;
}

int tup_entry_in_list(struct tup_entry *tent)
{
	return !(tent->list.next == NULL);
}

void tup_entry_add_ghost_list(struct tup_entry *tent, struct list_head *list)
{
	if(tent->type == TUP_NODE_GHOST) {
		/* It is fine if the ghost is already in the list - just make
		 * sure we don't try to list_add it twice.
		 */
		if(tent->ghost_list.next == NULL) {
			/* Use list_add_tail so new ghosts go to the back. This
			 * way if we need to re-check a directory, we will only
			 * check it again at the end (in case multiple ghosts
			 * are in a ghost directory, it would be silly to
			 * check one ghost, then check the dir, then check the
			 * next ghost, then check the dir again, etc).
			 */
			list_add_tail(&tent->ghost_list, list);
		}
	}
}

int tup_entry_del_ghost_list(struct tup_entry *tent)
{
	if(tent->ghost_list.next == NULL) {
		fprintf(stderr, "tup internal error: ghost_list.next is NULL in tup_entry_del_ghost_list %lli [%lli:%s]\n", tent->tnode.tupid, tent->dt, tent->name.s);
		return -1;
	}
	list_del(&tent->ghost_list);
	tent->ghost_list.next = NULL;
	return 0;
}

int tup_entry_debug_add_all_ghosts(struct list_head *list)
{
	struct rb_node *rbn;

	for(rbn = rb_first(&tup_tree); rbn; rbn = rb_next(rbn)) {
		struct tupid_tree *tt;
		struct tup_entry *tent;

		tt = rb_entry(rbn, struct tupid_tree, rbn);
		tent = container_of(tt, struct tup_entry, tnode);

		tup_entry_add_ghost_list(tent, list);
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
	struct rb_node *rbn;

	printf("Tup entries:\n");
	for(rbn = rb_first(&tup_tree); rbn; rbn = rb_next(rbn)) {
		struct tupid_tree *tt;
		struct tup_entry *tent;

		tt = rb_entry(rbn, struct tupid_tree, rbn);
		tent = container_of(tt, struct tup_entry, tnode);
		printf("  [%lli, dir=%lli, type=%i] name=%s\n", tent->tnode.tupid, tent->dt, tent->type, tent->name.s);
	}
}
