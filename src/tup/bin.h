#include "linux/list.h"
#include "tupid.h"

struct bin_list {
	struct list_head bins;
};

struct bin {
	struct list_head list;
	const char *name;
	struct list_head entries;
};

struct bin_entry {
	struct list_head list;
	char *name;
	int len;
	tupid_t tupid;
};

int bin_list_init(struct bin_list *bl);
void bin_list_del(struct bin_list *bl);
struct bin *bin_add(const char *name, struct bin_list *bl);
struct bin *bin_find(const char *name, struct bin_list *bl);
int bin_add_entry(struct bin *b, const char *name, int len, tupid_t tupid);

void dump_bins(struct bin_list *bl);
