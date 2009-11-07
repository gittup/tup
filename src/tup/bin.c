#include "bin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int bin_list_init(struct bin_list *bl)
{
	INIT_LIST_HEAD(&bl->bins);
	return 0;
}

void bin_list_del(struct bin_list *bl)
{
	while(!list_empty(&bl->bins)) {
		struct bin *b;
		b = list_entry(bl->bins.next, struct bin, list);

		while(!list_empty(&b->entries)) {
			struct bin_entry *be;
			be = list_entry(b->entries.next, struct bin_entry, list);
			list_del(&be->list);
			free(be->path);
			free(be);
		}
		list_del(&b->list);
		free(b);
	}
}

struct bin *bin_add(const char *name, struct bin_list *bl)
{
	struct bin *b;

	b = bin_find(name, bl);
	if(b)
		return b;

	b = malloc(sizeof *b);
	if(!b) {
		perror("malloc");
		return NULL;
	}
	b->name = name;
	INIT_LIST_HEAD(&b->entries);
	list_add(&b->list, &bl->bins);
	return b;
}

struct bin *bin_find(const char *name, struct bin_list *bl)
{
	struct bin *b;

	list_for_each_entry(b, &bl->bins, list) {
		if(strcmp(b->name, name) == 0) {
			return b;
		}
	}
	return NULL;
}

int bin_add_entry(struct bin *b, const char *path, int len,
		  struct tup_entry *tent)
{
	struct bin_entry *be;

	be = malloc(sizeof *be);
	if(!be) {
		perror("malloc");
		return -1;
	}
	be->path = malloc(len + 1);
	if(!be->path) {
		perror("malloc");
		return -1;
	}
	memcpy(be->path, path, len);
	be->path[len] = 0;
	be->len = len;

	be->tent = tent;
	list_add_tail(&be->list, &b->entries);
	return 0;
}
