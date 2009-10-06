#define _ATFILE_SOURCE
#include "entry.h"
#include "config.h"
#include "db.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

static struct rb_root tup_tree = RB_ROOT;

static struct tup_entry *new_entry(tupid_t tupid, tupid_t dt, tupid_t sym,
				   const char *name, int len, int type,
				   time_t mtime);
static int tup_entry_add_null(tupid_t tupid, struct tup_entry **dest);
static int tup_entry_add_m1(tupid_t tupid, struct tup_entry **dest);
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
	tent->entries.rb_node = NULL;

	if(tup_db_fill_tup_entry(tupid, tent) < 0)
		return -1;

	if(tup_entry_add_null(tent->dt, &tent->parent) < 0)
		return -1;
	if(tup_entry_add_m1(tent->sym_tupid, &tent->sym) < 0)
		return -1;

	if(tupid_tree_insert(&tup_tree, &tent->tnode) < 0) {
		fprintf(stderr, "tup error: Unable to insert node %lli into the tupid tree\n", tent->tnode.tupid);
		return -1;
	}
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

	/* TODO: This should be unnecessary */
	if(tup_entry_add(dt, NULL) < 0)
		return -1;

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
	tupid_tree_rm(&tup_tree, &tent->tnode);
	if(tent->parent) {
		string_tree_rm(&tent->parent->entries, &tent->name);
	}
	if(tent->entries.rb_node != NULL) {
		fprintf(stderr, "tup internal error: tup_entry_rm called on tupid %lli, which still has entries\n", tupid);
		return -1;
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
		fprintf(stderr, "tup error: Unable to find tup entry %lli in tup_entry_get()\n", tupid);
		return NULL;
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

void print_tup_entry(struct tup_entry *tent)
{
	/* Skip empty entries, and skip '.' here (dirt->parent == NULL) */
	if(!tent || !tent->parent)
		return;
	print_tup_entry(tent->parent);
	printf("%s/", tent->name.s);
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

static int tup_entry_add_m1(tupid_t tupid, struct tup_entry **dest)
{
	if(tupid == -1) {
		if(dest)
			*dest = NULL;
		return 0;
	}
	return tup_entry_add(tupid, dest);
}

int tup_entry_add_to_dir(tupid_t dt, tupid_t tupid, const char *name, int len,
			 int type, tupid_t sym, time_t mtime)
{
	struct tup_entry *tent;

	tent = new_entry(tupid, dt, sym, name, len, type, mtime);
	if(!tent)
		return -1;
	/* TODO: This should be unnecessary */
	if(tup_entry_add(dt, NULL) < 0)
		return -1;

	if(resolve_parent(tent) < 0)
		return -1;
	return 0;
}

int tup_entry_add_all(tupid_t tupid, tupid_t dt, int type, tupid_t sym,
		      time_t mtime, const char *name, struct rb_root *tree)
{
	struct tup_entry *tent;

	tent = new_entry(tupid, dt, sym, name, strlen(name), type, mtime);
	if(!tent)
		return -1;

	if(tupid_tree_add(tree, tupid) < 0)
		return -1;
	return 0;
}

int tup_entry_resolve_dirsym(void)
{
	struct rb_node *rbn;

	/* Resolve parents first, since those will all already be loaded into
	 * the tree. Then resolve symlinks, since that may require loading
	 * additional nodes (for example, a node may point to a ghost node).
	 * In that case we'll need an already stable tup_tree before calling
	 * tup_entry_add().
	 */
	for(rbn = rb_first(&tup_tree); rbn; rbn = rb_next(rbn)) {
		struct tupid_tree *tnode = rb_entry(rbn, struct tupid_tree, rbn);
		struct tup_entry *tent = container_of(tnode, struct tup_entry, tnode);

		if(resolve_parent(tent) < 0)
			return -1;
	}
	for(rbn = rb_first(&tup_tree); rbn; rbn = rb_next(rbn)) {
		struct tupid_tree *tnode = rb_entry(rbn, struct tupid_tree, rbn);
		struct tup_entry *tent = container_of(tnode, struct tup_entry, tnode);
		if(tup_entry_resolve_sym(tent) < 0)
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
	int dfd;
	int newdfd;

	if(tent->parent == NULL)
		return dup(tup_top_fd());

	dfd = tup_entry_open(tent->parent);
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

static struct tup_entry *new_entry(tupid_t tupid, tupid_t dt, tupid_t sym,
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
	tent->dt = dt;
	tent->sym_tupid = sym;
	tent->parent = NULL;
	tent->sym = NULL;
	tent->type = type;
	tent->mtime = mtime;
	tent->name.s = malloc(len+1);
	if(!tent->name.s) {
		perror("malloc");
		return NULL;
	}
	strncpy(tent->name.s, name, len);
	tent->name.s[len] = 0;
	tent->name.len = len;
	tent->entries.rb_node = NULL;

	if(tupid_tree_insert(&tup_tree, &tent->tnode) < 0) {
		fprintf(stderr, "tup error: Unable to insert node %lli into the tupid tree\n", tent->tnode.tupid);
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

int tup_entry_resolve_sym(struct tup_entry *tent)
{
	if(tent->sym_tupid < 0) {
		tent->sym = NULL;
	} else {
		if(tup_entry_add(tent->sym_tupid, &tent->sym) < 0)
			return -1;
		if(!tent->sym) {
			fprintf(stderr, "tup error: Unable to find sym entry [%lli] for node %lli\n", tent->sym_tupid, tent->tnode.tupid);
			return -1;
		}
	}
	return 0;
}

int tup_entry_change_name(tupid_t tupid, const char *new_name)
{
	struct tup_entry *tent;

	tent = tup_entry_get(tupid);
	if(!tent)
		return -1;
	return change_name(tent, new_name);
}

int tup_entry_change_name_dt(tupid_t tupid, const char *new_name,
			     tupid_t new_dt)
{
	struct tup_entry *tent;

	tent = tup_entry_get(tupid);
	if(!tent)
		return -1;

	tent->dt = new_dt;

	return change_name(tent, new_name);
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
