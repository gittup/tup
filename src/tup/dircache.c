#include "dircache.h"
#include "container.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dircache_init(struct dircache_root *droot)
{
	RB_INIT(&droot->wd_root);
	RB_INIT(&droot->dt_root);
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
	tupid_tree_insert(&droot->wd_root, &dc->wd_node);
	tupid_tree_insert(&droot->dt_root, &dc->dt_node);
	return;
}

void dircache_del(struct dircache_root *droot, struct dircache *dc)
{
	tupid_tree_rm(&droot->wd_root, &dc->wd_node);
	tupid_tree_rm(&droot->dt_root, &dc->dt_node);
	free(dc);
}

struct dircache *dircache_lookup_wd(struct dircache_root *droot, int wd)
{
	struct tupid_tree *tnode;

	tnode = tupid_tree_search(&droot->wd_root, wd);
	if(!tnode)
		return NULL;
	return container_of(tnode, struct dircache, wd_node);
}

struct dircache *dircache_lookup_dt(struct dircache_root *droot, tupid_t dt)
{
	struct tupid_tree *tnode;

	tnode = tupid_tree_search(&droot->dt_root, dt);
	if(!tnode)
		return NULL;
	return container_of(tnode, struct dircache, dt_node);
}
