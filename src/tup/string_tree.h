#ifndef tup_string_tree
#define tup_string_tree

#include "linux/rbtree.h"

struct string_tree {
	struct rb_node rbn;
	char *s;
	int len;
};

struct string_tree *string_tree_search(struct rb_root *root, const char *s,
				       int n);
struct string_tree *string_tree_search2(struct rb_root *root, const char *s1,
					int s1len, const char *s2);
int string_tree_insert(struct rb_root *root, struct string_tree *data);
static inline void string_tree_rm(struct rb_root *root, struct string_tree *st)
{
	rb_erase(&st->rbn, root);
}

/* _add is like _insert, but also malloc()s and copies 's' into 'st->s'. _free
 * just free()s st->s and calls _rm.
 */
int string_tree_add(struct rb_root *root, struct string_tree *st, const char *s);
void string_tree_free(struct rb_root *root, struct string_tree *st);

#endif
