#include "dircache.h"
#include "debug.h"
#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct dircache {
	struct list_head list;
	int wd;
	char *path;
};

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

	dump_dircache();
	return;
}

void dircache_del(int wd)
{
	struct dircache *dc;
	DEBUGP("del %i\n", wd);
	list_for_each_entry(dc, &dclist, list) {
		if(dc->wd == wd) {
			list_del(&dc->list);
			free(dc->path);
			free(dc);
			dump_dircache();
			return;
		}
	}
	fprintf(stderr, "dircache_del: entry not found.\n");
}

const char *dircache_lookup(int wd)
{
	/* TODO: Make efficient: use same hash algorithm in wrapper? */
	struct dircache *dc;
	list_for_each_entry(dc, &dclist, list) {
		if(dc->wd == wd)
			return dc->path;
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
