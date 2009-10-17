#ifndef entry_h
#define entry_h

#include "tupid_tree.h"
#include "string_tree.h"
#include <time.h>

/* Local cache of the entries in the 'node' database table */
struct tup_entry {
	struct tupid_tree tnode;
	tupid_t dt;
	tupid_t sym_tupid;
	struct tup_entry *parent;
	struct tup_entry *sym;
	int type;
	time_t mtime;
	struct string_tree name;
	struct rb_root entries;
};

int tup_entry_add(tupid_t tupid, struct tup_entry **dest);
int tup_entry_find_name_in_dir(tupid_t dt, const char *name, int len,
			       struct tup_entry **dest);
int tup_entry_add_to_dir(tupid_t dt, tupid_t tupid, const char *name, int len,
			 int type, tupid_t sym, time_t mtime);
int tup_entry_add_all(tupid_t tupid, tupid_t dt, int type, tupid_t sym,
		      time_t mtime, const char *name, struct rb_root *root);
int tup_entry_resolve_dirsym(void);
int tup_entry_resolve_sym(struct tup_entry *tent);
int tup_entry_change_name(tupid_t tupid, const char *new_name);
int tup_entry_change_name_dt(tupid_t tupid, const char *new_name, tupid_t dt);
int tup_entry_open_tupid(tupid_t tupid);
int tup_entry_open(struct tup_entry *tent);
int tup_entry_rm(tupid_t tupid);
struct tup_entry *tup_entry_get(tupid_t tupid);
struct tup_entry *tup_entry_find(tupid_t tupid);
int tup_entry_sym_follow(struct tup_entry **entry, struct rb_root *tree);
void print_tup_entry(struct tup_entry *tent);

#endif
