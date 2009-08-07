#include "dircache.h"
#include "linux/rbtree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dircache_add(struct rb_root *tree, int wd, tupid_t dt)
{
	struct dircache *dc;

	dc = dircache_lookup(tree, wd);
	if(dc) {
		dircache_del(tree, dc);
	}

	dc = malloc(sizeof *dc);
	if(!dc) {
		fprintf(stderr, "Out of memory.\n");
		return;
	}

	dc->dt = dt;
	dc->tnode.tupid = wd;
	tupid_tree_insert(tree, &dc->tnode);
	return;
}

void dircache_del(struct rb_root *tree, struct dircache *dc)
{
	rb_erase(&dc->tnode.rbn, tree);
	free(dc);
}

struct dircache *dircache_lookup(struct rb_root *tree, int wd)
{
	struct tupid_tree *tnode;

	tnode = tupid_tree_search(tree, wd);
	if(!tnode)
		return NULL;
	return container_of(tnode, struct dircache, tnode);
}
