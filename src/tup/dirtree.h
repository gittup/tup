#ifndef tup_dirtree_h
#define tup_dirtree_h

#include "tupid_tree.h"

struct dirtree {
	struct tupid_tree tnode;
	struct dirtree *parent;
	char *name;
};

int dirtree_add(tupid_t dt, struct dirtree **dest);
struct dirtree *dirtree_find(tupid_t dt);
int dirtree_open(tupid_t dt);

#endif
