#ifndef tup_tupid_tree
#define tup_tupid_tree

#include "linux/rbtree.h"
#include "tupid.h"

struct tupid_tree {
	struct rb_node rbn;
	tupid_t tupid;
};

struct tupid_tree *tupid_tree_search(struct rb_root *root, tupid_t tupid);
int tupid_tree_insert(struct rb_root *root, struct tupid_tree *data);

#endif
