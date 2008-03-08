#include "dircache.h"
#include "tup/debug.h"
#include "tup/list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void dump_dircache(void);
static LIST_HEAD(dclist);

void dircache_add(int wd, char *path)
{
	struct dircache *dc = malloc(sizeof *dc);
	if(!dc) {
		fprintf(stderr, "Out of memory.\n");
		return;
	}

	DEBUGP("add %i:'%s'\n", wd, path);

	dc->wd = wd;
	dc->path = path;
	list_add(&dc->list, &dclist);
	return;
}

void dircache_del(struct dircache *dc)
{
	DEBUGP("del %i\n", dc->wd);
	list_del(&dc->list);
	free(dc->path);
	free(dc);
	if(0)
		dump_dircache();
}

struct dircache *dircache_lookup(int wd)
{
	/* TODO: Make efficient: use same hash algorithm in wrapper? */
	struct dircache *dc;
	list_for_each_entry(dc, &dclist, list) {
		if(dc->wd == wd)
			return dc;
	}
	return NULL;
}

static void dump_dircache(void)
{
	struct dircache *dc;

	printf("Dircache:\n");
	list_for_each_entry(dc, &dclist, list) {
		printf("  %i: %s\n", dc->wd, dc->path);
	}
	printf("\n");
}
