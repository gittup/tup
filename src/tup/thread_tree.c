#include "thread_tree.h"

struct thread_tree *thread_tree_search(struct thread_root *troot, int id)
{
	struct rb_node *node;
	struct thread_tree *ret = NULL;

	pthread_mutex_lock(&troot->lock);
	node = troot->root.rb_node;
	while (node) {
		struct thread_tree *data = rb_entry(node, struct thread_tree, rbn);
		int result;

		result = id - data->id;

		if (result < 0)
			node = node->rb_left;
		else if (result > 0)
			node = node->rb_right;
		else {
			ret = data;
			break;
		}
	}
	pthread_mutex_unlock(&troot->lock);
	return ret;
}

int thread_tree_insert(struct thread_root *troot, struct thread_tree *data)
{
	struct rb_node **new;
	struct rb_node *parent = NULL;

	pthread_mutex_lock(&troot->lock);
	new = &(troot->root.rb_node);
	/* Figure out where to put new node */
	while (*new) {
		struct thread_tree *this = rb_entry(*new, struct thread_tree, rbn);
		int result = data->id - this->id;

		parent = *new;
		if (result < 0)
			new = &((*new)->rb_left);
		else if (result > 0)
			new = &((*new)->rb_right);
		else {
			pthread_mutex_unlock(&troot->lock);
			return -1;
		}
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->rbn, parent, new);
	rb_insert_color(&data->rbn, &troot->root);

	pthread_mutex_unlock(&troot->lock);
	return 0;
}

void thread_tree_rm(struct thread_root *troot, struct thread_tree *data)
{
	pthread_mutex_lock(&troot->lock);
	rb_erase(&data->rbn, &troot->root);
	pthread_mutex_unlock(&troot->lock);
}
