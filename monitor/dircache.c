#include "dircache.h"
#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct dircache {
	struct list_head list;
	int wd;
	char *path;
};

static LIST_HEAD(dclist);

void dircache_add(int wd, const char *path)
{
	struct dircache *dc = malloc(sizeof *dc);
	if(!dc) {
		fprintf(stderr, "Out of memory.\n");
		return;
	}

	dc->wd = wd;
	dc->path = malloc(strlen(path) + 1);
	if(!dc->path) {
		fprintf(stderr, "Out of memory.\n");
		goto err_path;
	}
	strcpy(dc->path, path);
	list_add(&dc->list, &dclist);
	return;

err_path:
	free(dc);
}

void dircache_del(int wd)
{
	struct dircache *dc;
	list_for_each_entry(dc, &dclist, list) {
		if(dc->wd == wd) {
			list_del(&dc->list);
			return;
		}
	}
	fprintf(stderr, "dircache_del: entry not found.\n");
}

const char *dircache_lookup(int wd)
{
	/* TODO: Make efficient */
	struct dircache *dc;
	list_for_each_entry(dc, &dclist, list) {
		if(dc->wd == wd)
			return dc->path;
	}
	return NULL;
}
