#ifndef tup_string_tree_h
#define tup_string_tree_h

#include "bsd/tree.h"

struct string_tree {
	RB_ENTRY(string_tree) linkage;
	char *s;
	int len;
};

RB_HEAD(string_entries, string_tree);
RB_PROTOTYPE(string_entries, string_tree, linkage, x);

int string_tree_insert(struct string_entries *root, struct string_tree *st);
struct string_tree *string_tree_search(struct string_entries *root, const char *s,
				       int len);

/* _add is like _insert, but also malloc()s and copies 's' into 'st->s'. _free
 * just free()s st->s and calls _rm.
 */
int string_tree_add(struct string_entries *root, struct string_tree *st, const char *s);
void string_tree_free(struct string_entries *root, struct string_tree *st);
static inline void string_tree_rm(struct string_entries *root, struct string_tree *st)
{
	RB_REMOVE(string_entries, root, st);
}

#endif
