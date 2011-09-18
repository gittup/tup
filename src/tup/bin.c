#include "bin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bin_list_del(struct bin_head *head)
{
	while(!LIST_EMPTY(head)) {
		struct bin *b = LIST_FIRST(head);

		while(!TAILQ_EMPTY(&b->entries)) {
			struct bin_entry *be = TAILQ_FIRST(&b->entries);
			TAILQ_REMOVE(&b->entries, be, list);
			free(be->path);
			free(be);
		}
		LIST_REMOVE(b, list);
		free(b);
	}
}

struct bin *bin_add(const char *name, struct bin_head *head)
{
	struct bin *b;

	b = bin_find(name, head);
	if(b)
		return b;

	b = malloc(sizeof *b);
	if(!b) {
		perror("malloc");
		return NULL;
	}
	b->name = name;
	TAILQ_INIT(&b->entries);
	LIST_INSERT_HEAD(head, b, list);
	return b;
}

struct bin *bin_find(const char *name, struct bin_head *head)
{
	struct bin *b;

	LIST_FOREACH(b, head, list) {
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
		free(be);
		return -1;
	}
	memcpy(be->path, path, len);
	be->path[len] = 0;
	be->len = len;

	be->tent = tent;
	TAILQ_INSERT_TAIL(&b->entries, be, list);
	return 0;
}
