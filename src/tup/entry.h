#ifndef entry_h
#define entry_h

#include "tupid_tree.h"
#include "string_tree.h"
#include "bsd/queue.h"
#include <stdio.h>
#include <time.h>

/* Local cache of the entries in the 'node' database table */
struct tup_entry {
	struct tupid_tree tnode;
	tupid_t dt;
	struct tup_entry *parent;
	int type;
	time_t mtime;
	struct string_tree name;
	struct string_entries entries;
	LIST_ENTRY(tup_entry) ghost_list;

	/* Only valid inside of get/release list. The next pointer is used to
	 * determine whether or not it is in the list (next==NULL means it
	 * isn't in the list).
	 */
	LIST_ENTRY(tup_entry) list;
};

LIST_HEAD(tup_entry_head, tup_entry);

int tup_entry_add(tupid_t tupid, struct tup_entry **dest);
int tup_entry_find_name_in_dir(tupid_t dt, const char *name, int len,
			       struct tup_entry **dest);
int tup_entry_add_to_dir(tupid_t dt, tupid_t tupid, const char *name, int len,
			 int type, time_t mtime,
			 struct tup_entry **dest);
int tup_entry_add_all(tupid_t tupid, tupid_t dt, int type,
		      time_t mtime, const char *name, struct tupid_entries *root);
int tup_entry_resolve_dirs(void);
int tup_entry_change_name(tupid_t tupid, const char *new_name);
int tup_entry_change_name_dt(tupid_t tupid, const char *new_name, tupid_t dt);
int tup_entry_open_tupid(tupid_t tupid);
int tup_entry_open(struct tup_entry *tent);
int tup_entry_openat(int root_dfd, struct tup_entry *tent);
int tup_entry_rm(tupid_t tupid);
struct tup_entry *tup_entry_get(tupid_t tupid);
struct tup_entry *tup_entry_find(tupid_t tupid);
int tup_entry_sym_follow(struct tup_entry *tent);
void tup_entry_set_verbose(int verbose);
void print_tup_entry(FILE *f, struct tup_entry *tent);
int snprint_tup_entry(char *dest, int len, struct tup_entry *tent);
int tup_entry_clear(void);
struct tup_entry_head *tup_entry_get_list(void);
void tup_entry_release_list(void);
void tup_entry_list_add(struct tup_entry *tent, struct tup_entry_head *head);
void tup_entry_list_del(struct tup_entry *tent);
int tup_entry_in_list(struct tup_entry *tent);
void tup_entry_add_ghost_list(struct tup_entry *tent, struct tup_entry_head *head);
int tup_entry_del_ghost_list(struct tup_entry *tent);
int tup_entry_debug_add_all_ghosts(struct tup_entry_head *head);
void dump_tup_entry(void);

#endif
