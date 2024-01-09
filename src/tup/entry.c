/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2009-2024  Mike Shal <marfey@gmail.com>
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
#include "mempool.h"
#include "config.h"
#include "db.h"
#include "compat.h"
#include "colors.h"
#include "container.h"
#include "variant.h"
#include "estring.h"
#include "tent_tree.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/stat.h>

static struct tupid_entries tup_root = RB_INITIALIZER(&tup_root);
static int do_verbose = 0;
static pthread_mutex_t entry_openat_mutex = PTHREAD_MUTEX_INITIALIZER;
static _Thread_local struct mempool pool = MEMPOOL_INITIALIZER(struct tup_entry);

static struct tup_entry *new_entry(tupid_t tupid, tupid_t dt,
				   const char *name, int len,
				   const char *display, int displaylen, const char *flags, int flagslen,
				   enum TUP_NODE_TYPE type,
				   struct timespec mtime, tupid_t srcid);
static int rm_entry(tupid_t tupid, int safe);
static int resolve_parent(struct tup_entry *tent);
static int change_name(struct tup_entry *tent, const char *new_name);

int tup_entry_add(tupid_t tupid, struct tup_entry **dest)
{
	struct tup_entry *tent;

	if(tupid < 0) {
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

	if(tup_db_fill_tup_entry(tupid, &tent) < 0)
		return -1;
	if(tent->dt > 0) {
		if(tup_entry_add(tent->dt, NULL) < 0)
			return -1;
	}
	if(resolve_parent(tent) < 0)
		return -1;
	if(dest)
		*dest = tent;
	return 0;
}

int tup_entry_find_name_in_dir_dt(struct tup_entry *dtent, const char *name, int len,
				  struct tup_entry **dest)
{
	if(len < 0)
		len = strlen(name);

	if(dtent->tnode.tupid == 0) {
		if(strncmp(name, ".", len) == 0) {
			*dest = tup_entry_find(DOT_DT);
			return 0;
		}
		fprintf(stderr, "tup error: entry '%.*s' shouldn't have dtent == NULL\n", len, name);
		return -1;
	}

	return tup_entry_find_name_in_dir(dtent, name, len, dest);
}

int tup_entry_find_name_in_dir(struct tup_entry *tent, const char *name, int len,
			       struct tup_entry **dest)
{
	struct string_tree *st;

	if(len < 0)
		len = strlen(name);

	st = string_tree_search(&tent->entries, name, len);
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

	tup_db_del_ghost_tree(tent);

	tupid_tree_rm(&tup_root, &tent->tnode);
	if(tent->parent) {
		string_tree_rm(&tent->parent->entries, &tent->name);
	}
	if(tent->re) {
		pcre2_code_free(tent->re);
	}
	free_tent_tree(&tent->stickies);
	free_tent_tree(&tent->group_stickies);
	if(tent->refcount != 0) {
		fprintf(stderr, "tup internal error: tup_entry_rm called on tupid %lli, which still has refcount=%i\n", tupid, tent->refcount);
		return -1;
	}
	free(tent->name.s);
	free(tent->display);
	free(tent->flags);
	mempool_free(&pool, tent);
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
	if(tup_entry_variant_null(tent) != tup_entry_variant_null(tent->parent)) {
		fprintf(f, "[%s] ", tent->name.s);
		return 0;
	}
	if(print_tup_entry_internal(f, tent->parent))
		fprintf(f, "%c", path_sep());
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
		if(tent->type == TUP_NODE_CMD) {
			fprintf(f, ": ");
		} else {
			fprintf(f, "%c", path_sep());
		}
	}
	if(tent->parent && tup_entry_variant_null(tent) != tup_entry_variant_null(tent->parent)) {
		fprintf(f, "[%s] ", tent->name.s);
		name = ".";
		name_sz = 1;
	} else {
		name = tent->name.s;
		name_sz = tent->name.len;
	}
	if(!do_verbose && tent->display) {
		name = tent->display;
		name_sz = tent->displaylen;
	}

	color_set(f);
	fprintf(f, "%s%s%.*s%s", color_type(tent->type), color_append_normal(), name_sz, name, color_end());
}

void print_tupid(FILE *f, tupid_t tupid)
{
	struct tup_entry *tent;
	if(tup_entry_add(tupid, &tent) < 0)
		return;
	print_tup_entry(f, tent);
}

int snprint_tup_entry(char *dest, int len, struct tup_entry *tent)
{
	int rc;
	if(!tent || !tent->parent) {
		if(len)
			*dest = 0;
		return 0;
	}
	rc = snprint_tup_entry(dest, len, tent->parent);
	rc += snprintf(dest + rc, len - rc, "/%s", tent->name.s);
	return rc;
}

int write_tup_entry(FILE *f, struct tup_entry *tent)
{
	/* Write out the tup entry unformatted, with OS-specific separators
	 * (unlike print_tup_entry, which is a pretty-print)
	 */
	if(!tent)
		return 0;
	if(tent->tnode.tupid == DOT_DT) {
		fprintf(f, ".");
		return 0;
	}
	if(tent->parent->tnode.tupid != DOT_DT) {
		if(write_tup_entry(f, tent->parent) < 0)
			return -1;
	}
	fprintf(f, "%s", tent->name.s);
	if(tent->type == TUP_NODE_DIR)
		fprintf(f, "%c", path_sep());
	return 0;
}

int tup_entry_add_to_dir(struct tup_entry *dtent, tupid_t tupid, const char *name, int len,
			 const char *display, int displaylen, const char *flags, int flagslen,
			 enum TUP_NODE_TYPE type, struct timespec mtime, tupid_t srcid,
			 struct tup_entry **dest)
{
	struct tup_entry *tent;

	tent = new_entry(tupid, dtent->tnode.tupid, name, len, display, displaylen, flags, flagslen, type, mtime, srcid);
	if(!tent)
		return -1;
	if(resolve_parent(tent) < 0)
		return -1;
	if(dest)
		*dest = tent;
	return 0;
}

int tup_entry_add_all(tupid_t tupid, tupid_t dt, enum TUP_NODE_TYPE type,
		      struct timespec mtime, tupid_t srcid, const char *name, const char *display, const char *flags,
		      struct tup_entry **dest)
{
	struct tup_entry *tent;

	tent = new_entry(tupid, dt, name, strlen(name), display, -1, flags, -1, type, mtime, srcid);
	if(!tent)
		return -1;
	if(dest)
		*dest = tent;
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

static int entry_openat_internal(int root_dfd, struct tup_entry *tent)
{
	int dfd;
	int newdfd;

	if(!tent)
		return -1;
	if(tent->parent == NULL) {
		return fcntl(root_dfd, F_DUPFD_CLOEXEC, 0);
	}

	dfd = entry_openat_internal(root_dfd, tent->parent);
	if(dfd < 0)
		return dfd;

	newdfd = openat(dfd, tent->name.s, O_RDONLY | O_CLOEXEC);
	if(newdfd < 0 && errno == ENOENT && tent->type == TUP_NODE_GENERATED_DIR) {
		if(mkdirat(dfd, tent->name.s, 0777) < 0) {
			perror(tent->name.s);
			close(dfd);
			return -1;
		}
		newdfd = openat(dfd, tent->name.s, O_RDONLY | O_CLOEXEC);
	}
	if(close(dfd) < 0) {
		perror("close(dfd)");
		return -1;
	}
	if(newdfd < 0) {
		if(errno == ENOENT || errno == ENOTDIR)
			return -errno;
		perror(tent->name.s);
		return -1;
	}
	return newdfd;
}

int tup_entry_open(struct tup_entry *tent)
{
	return tup_entry_openat(tup_top_fd(), tent);
}

int tup_entry_openat(int root_dfd, struct tup_entry *tent)
{
	int rc;
	/* This mutex protects against multiple tup_entry_open/openat calls
	 * from trying to create generated directories at the same time.
	 * (t4112)
	 */
	pthread_mutex_lock(&entry_openat_mutex);
	rc = entry_openat_internal(root_dfd, tent);
	pthread_mutex_unlock(&entry_openat_mutex);
	return rc;
}

void tup_entry_add_ref(struct tup_entry *tent)
{
	tent->refcount++;
}

void tup_entry_del_ref(struct tup_entry *tent)
{
	tent->refcount--;
}

struct variant *tup_entry_variant(struct tup_entry *tent)
{
	/* The variant field isn't set when we initially create tup_entrys, since
	 * if we are doing tup_entry_add_all, we may not have the tup.config
	 * entry until the end. It also doesn't make sense to add that entry
	 * until we have scanned for a real tup.config node. Instead we just
	 * use this function to get the variant field, since the tup_entry tree
	 * will contain the necessary directory structure at this point. Then
	 * each entry has the same variant as its parent, until a tup.config node
	 * is found.
	 */
	if(!tent->variant) {
		tent->variant = tup_entry_variant_null(tent);
		if(!tent->variant) {
			fprintf(stderr, "tup internal error: Unable to set tent->variant for tup entry: ");
			print_tup_entry(stderr, tent);
			fprintf(stderr, "\n");
			exit(1);
		}
	}
	return tent->variant;
}

struct variant *tup_entry_variant_null(struct tup_entry *tent)
{
	if(!tent->variant) {
		tent->variant = variant_search(tent->tnode.tupid);
		if(!tent->variant) {
			if(tent->parent) {
				tent->variant = tup_entry_variant_null(tent->parent);
			}
		}
	}
	return tent->variant;
}

tupid_t tup_entry_vardt(struct tup_entry *tent)
{
	return tup_entry_variant(tent)->tent->tnode.tupid;
}

static int set_string(char **dest, int *destlen, const char *src, int srclen)
{
	if(src) {
		*dest = malloc(srclen+1);
		if(!*dest) {
			perror("malloc");
			return -1;
		}
		strncpy(*dest, src, srclen);
		(*dest)[srclen] = 0;
		*destlen = srclen;
	} else {
		*dest = NULL;
		*destlen = 0;
	}
	return 0;
}

static struct tup_entry *new_entry(tupid_t tupid, tupid_t dt,
				   const char *name, int len,
				   const char *display, int displaylen, const char *flags, int flagslen,
				   enum TUP_NODE_TYPE type,
				   struct timespec mtime, tupid_t srcid)
{
	struct tup_entry *tent;

	tent = mempool_alloc(&pool);
	if(!tent) {
		return NULL;
	}

	if(len == -1)
		len = strlen(name);
	if(displaylen == -1) {
		if(display)
			displaylen = strlen(display);
		else
			displaylen = 0;
	}
	if(flagslen == -1) {
		if(flags)
			flagslen = strlen(flags);
		else
			flagslen = 0;
	}

	tent->tnode.tupid = tupid;
	tent->dt = dt;
	tent->parent = NULL;
	tent->type = type;
	tent->mtime = mtime;
	tent->srcid = srcid;
	tent->variant = NULL;
	tent_tree_init(&tent->stickies);
	tent_tree_init(&tent->group_stickies);
	tent->retrieved_stickies = 0;
	tent->incoming = NULL;
	tent->refcount = 0;
	if(set_string(&tent->name.s, &tent->name.len, name, len) < 0)
		return NULL;
	if(set_string(&tent->display, &tent->displaylen, display, displaylen) < 0)
		return NULL;
	if(set_string(&tent->flags, &tent->flagslen, flags, flagslen) < 0)
		return NULL;
	RB_INIT(&tent->entries);

	if(tent->dt == exclusion_dt()) {
		int error;
		size_t erroffset;
		tent->re = pcre2_compile((PCRE2_SPTR)tent->name.s, PCRE2_ZERO_TERMINATED, 0, &error, &erroffset, NULL);
		if(!tent->re) {
			PCRE2_UCHAR buffer[256];
			pcre2_get_error_message(error, buffer, sizeof(buffer));
			fprintf(stderr, "tup error: Unable to compile regular expression '%s' at offset %zi: %s\n", tent->name.s, erroffset, buffer);
			return NULL;
		}
	} else {
		tent->re = NULL;
	}

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

int tup_entry_change_name_dt(tupid_t tupid, const char *new_name,
			     tupid_t new_dt)
{
	struct tup_entry *tent;

	tent = tup_entry_get(tupid);
	tent->dt = new_dt;

	return change_name(tent, new_name);
}

int tup_entry_change_display(struct tup_entry *tent, const char *display, int displaylen)
{
	free(tent->display);
	if(set_string(&tent->display, &tent->displaylen, display, displaylen) < 0)
		return -1;
	return 0;
}

int tup_entry_change_flags(struct tup_entry *tent, const char *flags, int flagslen)
{
	free(tent->flags);
	if(set_string(&tent->flags, &tent->flagslen, flags, flagslen) < 0)
		return -1;
	return 0;
}

int tup_entry_clear(void)
{
	/* First delete all stickies & group_stickies. Even though these are
	 * freed in rm_entry(), the refcount check would fail since we don't
	 * delete nodes in any particular order. And unlike the entries tree
	 * for directory entries, we don't have a way to remove a node from all
	 * places where it might point to as a sticky link when it gets
	 * deleted.
	 */
	struct tupid_tree *tt;
	RB_FOREACH(tt, tupid_entries, &tup_root) {
		struct tup_entry *tent = container_of(tt, struct tup_entry, tnode);
		free_tent_tree(&tent->stickies);
		free_tent_tree(&tent->group_stickies);
	}

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
		struct tupid_tree *tmp;

		RB_FOREACH_SAFE(tt, tupid_entries, &tup_root, tmp) {
			if(rm_entry(tt->tupid, 1) < 0)
				return -1;
		}
	}
	return 0;
}

int tup_entry_add_ghost_tree(struct tent_entries *root, struct tup_entry *tent)
{
	if(tent->type == TUP_NODE_GHOST || tent->type == TUP_NODE_GROUP ||
	   tent->type == TUP_NODE_GENERATED_DIR) {
		if(tent_tree_add_dup(root, tent) < 0)
			return -1;
	}
	return 0;
}

int tup_entry_debug_add_all_ghosts(struct tent_entries *root)
{
	struct tupid_tree *tt;

	RB_FOREACH(tt, tupid_entries, &tup_root) {
		struct tup_entry *tent;

		tent = container_of(tt, struct tup_entry, tnode);
		if(tup_entry_add_ghost_tree(root, tent) < 0)
			return -1;
	}
	return 0;
}

int tup_entry_get_dir_tree(struct tup_entry *tent, struct tupid_entries *root)
{
	struct string_tree *st;
	RB_FOREACH(st, string_entries, &tent->entries) {
		struct tup_entry *subtent;
		subtent = container_of(st, struct tup_entry, name);
		if(subtent->type != TUP_NODE_GHOST &&
		   subtent->type != TUP_NODE_CMD &&
		   !is_virtual_tent(subtent))
			if(tupid_tree_add(root, subtent->tnode.tupid) < 0)
				return -1;
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

static int get_full_path_tents(tupid_t tupid, struct tent_list_head *head)
{
	struct tup_entry *tent;

	tent = tup_entry_get(tupid);
	while(tent) {
		if(tent_list_add_head(head, tent) < 0)
			return -1;

		tent = tent->parent;
	}
	return 0;
}

int get_relative_dir(FILE *f, struct estring *e, tupid_t start, tupid_t end)
{
	/* For resource files, always use '/' as the separator.  Both cl and
	 * cygwin can handle '/', but cygwin can't handle '\'.
	 */
	return get_relative_dir_sep(f, e, start, end, '/');
}

int get_relative_dir_sep(FILE *f, struct estring *e, tupid_t start, tupid_t end, char sep)
{
	struct tent_list_head startlist;
	struct tent_list_head endlist;
	struct tent_list *startentry;
	struct tent_list *endentry;
	char sep_str[2] = {sep, 0};
	int first = 0;

	tent_list_init(&startlist);
	tent_list_init(&endlist);
	if(get_full_path_tents(start, &startlist) < 0)
		return -1;
	if(get_full_path_tents(end, &endlist) < 0)
		return -1;

	while(!tent_list_empty(&startlist) && !tent_list_empty(&endlist)) {
		startentry = tent_list_first(&startlist);
		endentry = tent_list_first(&endlist);

		if(startentry->tent == endentry->tent) {
			tent_list_delete(&startlist, startentry);
			tent_list_delete(&endlist, endentry);
		} else {
			break;
		}
	}

	tent_list_foreach(startentry, &startlist) {
		if(!first) {
			first = 1;
		} else {
			if(f)
				fprintf(f, "%s", sep_str);
			if(e)
				if(estring_append(e, sep_str, 1) < 0)
					return -1;
		}
		if(f)
			fprintf(f, "..");
		if(e)
			if(estring_append(e, "..", 2) < 0)
				return -1;
	}
	tent_list_foreach(endentry, &endlist) {
		if(!first) {
			first = 1;
		} else {
			if(f)
				fprintf(f, "%s", sep_str);
			if(e)
				if(estring_append(e, sep_str, 1) < 0)
					return -1;
		}
		if(f)
			fprintf(f, "%s", endentry->tent->name.s);
		if(e)
			if(estring_append(e, endentry->tent->name.s, endentry->tent->name.len) < 0)
				return -1;
	}
	if(!first) {
		if(f)
			fprintf(f, ".");
		if(e)
			if(estring_append(e, ".", 1) < 0)
				return -1;
	}

	free_tent_list(&endlist);
	free_tent_list(&startlist);
	return 0;
}

static int has_flag(struct tup_entry *tent, char c)
{
	return memchr(tent->flags, c, tent->flagslen) != NULL;
}

int is_transient_tent(struct tup_entry *tent)
{
	return has_flag(tent, 't');
}

int is_compiledb_tent(struct tup_entry *tent)
{
	return has_flag(tent, 'j');
}

int exclusion_match(FILE *f, struct tent_entries *exclusion_root, const char *s, struct tup_entry **match)
{
	struct tent_tree *tt;
	int len = strlen(s);

	*match = NULL;
	RB_FOREACH(tt, tent_entries, exclusion_root) {
		int rc;
		pcre2_match_data *re_match = pcre2_match_data_create_from_pattern(tt->tent->re, NULL);
		rc = pcre2_match(tt->tent->re, (PCRE2_SPTR)s, len, 0, 0, re_match, NULL);
		pcre2_match_data_free(re_match);
		if(rc >= 0) {
			*match = tt->tent;
			if(do_verbose) {
				fprintf(f, "tup info: Ignoring file '%s' because it matched the regex '%s'\n", s, tt->tent->name.s);
			}
			break;
		} else if(rc != PCRE2_ERROR_NOMATCH) {
			fprintf(f, "tup error: Regex failed to execute: %s\n", tt->tent->name.s);
			return -1;
		}
	}
	return 0;
}
