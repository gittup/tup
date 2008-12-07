#include "dircache.h"
#include "debug.h"
#include "list.h"
#include "memdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void dump_dircache(void);
static LIST_HEAD(dclist);

void dircache_add(struct memdb *m, int wd, char *path, tupid_t dt)
{
	struct dircache *dc;

	dc = dircache_lookup(m, wd);
	if(dc) {
		dircache_del(m, dc);
	}

	dc = malloc(sizeof *dc);
	if(!dc) {
		fprintf(stderr, "Out of memory.\n");
		return;
	}

	DEBUGP("add %i:'%s'\n", wd, path);

	dc->wd = wd;
	dc->dt = dt;
	dc->path = path;
	list_add(&dc->list, &dclist);
	memdb_add(m, wd, dc);
	return;
}

void dircache_del(struct memdb *m, struct dircache *dc)
{
	DEBUGP("del %i\n", dc->wd);
	memdb_remove(m, dc->wd);
	list_del(&dc->list);
	free(dc->path);
	free(dc);
	if(0)
		dump_dircache();
}

struct dircache *dircache_lookup(const struct memdb *m, int wd)
{
	return memdb_find(m, wd);
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
