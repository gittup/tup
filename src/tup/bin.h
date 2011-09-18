#ifndef tup_bin_h
#define tup_bin_h

#include "bsd/queue.h"
#include "tupid.h"

struct tup_entry;

struct bin_entry {
	TAILQ_ENTRY(bin_entry) list;
	char *path;
	int len;
	struct tup_entry *tent;
};
TAILQ_HEAD(bin_entry_head, bin_entry);

struct bin {
	LIST_ENTRY(bin) list;
	const char *name;
	struct bin_entry_head entries;
};
LIST_HEAD(bin_head, bin);

void bin_list_del(struct bin_head *head);
struct bin *bin_add(const char *name, struct bin_head *head);
struct bin *bin_find(const char *name, struct bin_head *head);
int bin_add_entry(struct bin *b, const char *path, int len,
		  struct tup_entry *tent);

#endif
