#include "tupid_tree.h"
#include "db.h"
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

int tupid_tree_add(struct rb_root *root, tupid_t tupid)
{
	struct tupid_tree *tt;

	tt = malloc(sizeof *tt);
	if(!tt) {
		perror("malloc");
		return -1;
	}
	tt->tupid = tupid;
	if(tupid_tree_insert(root, tt) < 0) {
		fprintf(stderr, "Error: Unable to insert duplicate tupid %lli\n", tupid);
		tup_db_print(stderr, tupid);
		return -1;
	}
	return 0;
}

int tupid_tree_add_cmdid(struct rb_root *root, tupid_t tupid, tupid_t cmdid)
{
	struct tupid_tree *tt;

	tt = malloc(sizeof *tt);
	if(!tt) {
		perror("malloc");
		return -1;
	}
	tt->tupid = tupid;
	if(tupid_tree_insert(root, tt) < 0) {
		fprintf(stderr, "Error: Duplicate input %lli found for command %lli\n", tupid, cmdid);
		tup_db_print(stderr, cmdid);
		tup_db_print(stderr, tupid);
		return -1;
	}
	return 0;
}

int tupid_tree_add_dup(struct rb_root *root, tupid_t tupid)
{
	struct tupid_tree *tt;

	tt = malloc(sizeof *tt);
	if(!tt) {
		perror("malloc");
		return -1;
	}
	tt->tupid = tupid;
	if(tupid_tree_insert(root, tt) < 0)
		free(tt);
	return 0;
}

void tupid_tree_remove(struct rb_root *root, tupid_t tupid)
{
	struct tupid_tree *tt;

	tt = tupid_tree_search(root, tupid);
	if(!tt) {
		return;
	}
	tupid_tree_rm(root, tt);
	free(tt);
}

void free_tupid_tree(struct rb_root *root)
{
	struct rb_node *rbn;

	while((rbn = rb_first(root)) != NULL) {
		struct tupid_tree *tt = rb_entry(rbn, struct tupid_tree, rbn);
		rb_erase(rbn, root);
		free(tt);
	}
}

int tree_entry_add(struct rb_root *tree, tupid_t tupid, int type, int *count)
{
	struct tree_entry *te;

	te = malloc(sizeof *te);
	if(!te) {
		perror("malloc");
		return -1;
	}
	te->tnode.tupid = tupid;
	te->type = type;
	if(tupid_tree_insert(tree, &te->tnode) < 0) {
		fprintf(stderr, "tup internal error: Duplicate tupid %lli in tree_entry_add?\n", tupid);
		return -1;
		free(te);
	} else {
		if(type == TUP_NODE_GENERATED && count)
			(*count)++;
	}
	return 0;
}

void tree_entry_remove(struct rb_root *tree, tupid_t tupid, int *count)
{
	struct tree_entry *te;
	struct tupid_tree *tt;

	tt = tupid_tree_search(tree, tupid);
	if(!tt)
		return;
	rb_erase(&tt->rbn, tree);
	te = container_of(tt, struct tree_entry, tnode);
	if(te->type == TUP_NODE_GENERATED && count)
		(*count)--;
	free(te);
}
