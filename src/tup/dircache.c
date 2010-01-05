#include "dircache.h"
#include "linux/rbtree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dircache_init(struct dircache_root *droot)
{
	droot->wd_tree.rb_node = NULL;
	droot->dt_tree.rb_node = NULL;
}

void dircache_add(struct dircache_root *droot, int wd, tupid_t dt)
{
	struct dircache *dc;

	dc = dircache_lookup_wd(droot, wd);
	if(dc) {
		dircache_del(droot, dc);
	}

	dc = malloc(sizeof *dc);
	if(!dc) {
		fprintf(stderr, "Out of memory.\n");
		return;
	}

	dc->wd_node.tupid = wd;
	dc->dt_node.tupid = dt;
	tupid_tree_insert(&droot->wd_tree, &dc->wd_node);
	tupid_tree_insert(&droot->dt_tree, &dc->dt_node);
	return;
}

void dircache_del(struct dircache_root *droot, struct dircache *dc)
{
	rb_erase(&dc->wd_node.rbn, &droot->wd_tree);
	rb_erase(&dc->dt_node.rbn, &droot->dt_tree);
	free(dc);
}

struct dircache *dircache_lookup_wd(struct dircache_root *droot, int wd)
{
	struct tupid_tree *tnode;

	tnode = tupid_tree_search(&droot->wd_tree, wd);
	if(!tnode)
		return NULL;
	return container_of(tnode, struct dircache, wd_node);
}

struct dircache *dircache_lookup_dt(struct dircache_root *droot, tupid_t dt)
{
	struct tupid_tree *tnode;

	tnode = tupid_tree_search(&droot->dt_tree, dt);
	if(!tnode)
		return NULL;
	return container_of(tnode, struct dircache, dt_node);
}
