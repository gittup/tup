#include "string_tree.h"
#include <string.h>

struct string_tree *string_tree_search(struct rb_root *root, const char *s,
				       int n)
{
	struct rb_node *node = root->rb_node;

	while (node) {
		struct string_tree *data = rb_entry(node, struct string_tree, rbn);
		int result;

		if(n == -1) {
			result = strcmp(s, data->s);
		} else {
			result = strncmp(s, data->s, n);
			if(result == 0)
				result = n - data->len;
		}

		if (result < 0)
			node = node->rb_left;
		else if (result > 0)
			node = node->rb_right;
		else
			return data;
	}
	return NULL;
}

struct string_tree *string_tree_search2(struct rb_root *root, const char *s1,
					int s1len, const char *s2)
{
	struct rb_node *node = root->rb_node;

	while (node) {
		struct string_tree *data = rb_entry(node, struct string_tree, rbn);
		int result;

		result = strncmp(s1, data->s, s1len);
		if(result == 0) {
			if(s2)
				result = strcmp(s2, data->s + s1len);
			else
				result = s1len - data->len;
		}

		if (result < 0)
			node = node->rb_left;
		else if (result > 0)
			node = node->rb_right;
		else
			return data;
	}
	return NULL;
}

int string_tree_insert(struct rb_root *root, struct string_tree *data)
{
	struct rb_node **new = &(root->rb_node), *parent = NULL;

	/* Figure out where to put new node */
	while (*new) {
		struct string_tree *this = rb_entry(*new, struct string_tree, rbn);
		int result = strcmp(data->s, this->s);

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
