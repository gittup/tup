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

void dump_dircache(struct rb_root *tree)
{
	struct rb_node *rbn;

	printf("Dircache:\n");
	for(rbn = rb_first(tree); rbn; rbn = rb_next(rbn)) {
		struct tupid_tree *tt;
		struct dircache *dc;
		tt = rb_entry(rbn, struct tupid_tree, rbn);
		dc = container_of(tt, struct dircache, tnode);
		printf("  Dc[tupid=%lli, wd=%lli]\n", dc->dt, dc->tnode.tupid);
	}
}
