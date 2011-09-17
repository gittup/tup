#include "tupid_tree.h"
#include "db.h"
#include <stdlib.h>

static int tupid_tree_cmp(struct tupid_tree *tt1, struct tupid_tree *tt2)
{
	return tt1->tupid - tt2->tupid;
}

RB_GENERATE(tupid_entries, tupid_tree, linkage, tupid_tree_cmp);

struct tupid_tree *tupid_tree_search(struct tupid_entries *root, tupid_t tupid)
{
	struct tupid_tree tt = {
		.tupid = tupid,
	};
	return RB_FIND(tupid_entries, root, &tt);
}

int tupid_tree_insert(struct tupid_entries *root, struct tupid_tree *data)
{
	if(RB_INSERT(tupid_entries, root, data) != NULL) {
		return -1;
	}
	return 0;
}

int tupid_tree_add(struct tupid_entries *root, tupid_t tupid)
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

int tupid_tree_add_dup(struct tupid_entries *root, tupid_t tupid)
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

int tupid_tree_copy(struct tupid_entries *dest, struct tupid_entries *src)
{
	struct tupid_tree *tt;
	RB_FOREACH(tt, tupid_entries, src) {
		if(tupid_tree_add(dest, tt->tupid) < 0)
			return -1;
	}
	return 0;
}

void tupid_tree_remove(struct tupid_entries *root, tupid_t tupid)
{
	struct tupid_tree *tt;

	tt = tupid_tree_search(root, tupid);
	if(!tt) {
		return;
	}
	tupid_tree_rm(root, tt);
	free(tt);
}

void free_tupid_tree(struct tupid_entries *root)
{
	struct tupid_tree *tt;

	while((tt = BSD_RB_ROOT(root)) != NULL) {
		tupid_tree_rm(root, tt);
		free(tt);
	}
}

int tree_entry_add(struct tupid_entries *root, tupid_t tupid, int type, int *count)
{
	struct tree_entry *te;

	te = malloc(sizeof *te);
	if(!te) {
		perror("malloc");
		return -1;
	}
	te->tnode.tupid = tupid;
	te->type = type;
	if(tupid_tree_insert(root, &te->tnode) < 0) {
		fprintf(stderr, "tup internal error: Duplicate tupid %lli in tree_entry_add?\n", tupid);
		free(te);
		return -1;
	} else {
		if(type == TUP_NODE_GENERATED && count)
			(*count)++;
	}
	return 0;
}

void tree_entry_remove(struct tupid_entries *root, tupid_t tupid, int *count)
{
	struct tree_entry *te;
	struct tupid_tree *tt;

	tt = tupid_tree_search(root, tupid);
	if(!tt)
		return;
	tupid_tree_rm(root, tt);
	te = container_of(tt, struct tree_entry, tnode);
	if(te->type == TUP_NODE_GENERATED && count)
		(*count)--;
	free(te);
}
