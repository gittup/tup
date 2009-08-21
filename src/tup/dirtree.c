#define _ATFILE_SOURCE
#include "dirtree.h"
#include "db.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

static struct rb_root tree = RB_ROOT;

static int do_open(struct dirtree *dirt);

int dirtree_add(tupid_t dt, struct dirtree **dest)
{
	struct dirtree *dirt;
	tupid_t parent;

	if(dt == 0) {
		if(dest)
			*dest = NULL;
		return 0;
	}

	dirt = dirtree_find(dt);
	if(dirt != NULL) {
		if(dest)
			*dest = dirt;
		return 0;
	}

	dirt = malloc(sizeof *dirt);
	if(!dirt) {
		perror("malloc");
		return -1;
	}
	dirt->tnode.tupid = dt;
	parent = tup_db_select_dirname(dt, &dirt->name);
	if(parent < 0) {
		fprintf(stderr, "tup error: Unable to find node entry for directory %lli\n", dt);
		return -1;
	}

	if(dirtree_add(parent, &dirt->parent) < 0)
		return -1;
	tupid_tree_insert(&tree, &dirt->tnode);
	if(dest)
		*dest = dirt;
	return 0;
}

struct dirtree *dirtree_find(tupid_t dt)
{
	struct tupid_tree *tnode;
	tnode = tupid_tree_search(&tree, dt);
	if(!tnode)
		return NULL;
	return container_of(tnode, struct dirtree, tnode);
}

int dirtree_open(tupid_t dt)
{
	struct dirtree *dirt;

	if(dirtree_add(dt, &dirt) < 0)
		return -1;
	if(!dirt) {
		fprintf(stderr, "tup error: dirtree(%lli) is NULL\n", dt);
		return -1;
	}
	return do_open(dirt);
}

static int do_open(struct dirtree *dirt)
{
	int dfd;
	int newdfd;

	if(dirt->parent == NULL)
		return dup(tup_top_fd());

	dfd = do_open(dirt->parent);
	if(dfd < 0)
		return dfd;

	newdfd = openat(dfd, dirt->name, O_RDONLY);
	close(dfd);
	if(newdfd < 0) {
		if(errno == ENOENT)
			return -ENOENT;
		perror(dirt->name);
		return -1;
	}
	return newdfd;
}
