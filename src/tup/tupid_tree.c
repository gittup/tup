#include "tupid_tree.h"
#include <stdlib.h>

struct tupid_tree *tupid_tree_search(struct rb_root *root, tupid_t tupid)
{
	struct rb_node *node = root->rb_node;

	while (node) {
		struct tupid_tree *data = rb_entry(node, struct tupid_tree, rbn);
		int result;

		result = tupid - data->tupid;

		if (result < 0)
			node = node->rb_left;
		else if (result > 0)
			node = node->rb_right;
		else
			return data;
	}
	return NULL;
}

int tupid_tree_insert(struct rb_root *root, struct tupid_tree *data)
{
	struct rb_node **new = &(root->rb_node), *parent = NULL;

	/* Figure out where to put new node */
	while (*new) {
		struct tupid_tree *this = rb_entry(*new, struct tupid_tree, rbn);
		int result = data->tupid - this->tupid;

		parent = *new;
		if (result < 0)
			new = &((*new)->rb_left);
		else if (result > 0)
			new = &((*new)->rb_right);
		else
			return -1;
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->rbn, parent, new);
	rb_insert_color(&data->rbn, root);

	return 0;
}

void tupid_tree_remove(struct rb_root *root, tupid_t tupid)
{
	struct tupid_tree *tt;

	tt = tupid_tree_search(root, tupid);
	if(!tt)
		return;
	rb_erase(&tt->rbn, root);
	free(tt);
}
