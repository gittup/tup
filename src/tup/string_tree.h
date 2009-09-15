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

#endif
