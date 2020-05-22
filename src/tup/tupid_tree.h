/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2009-2020  Mike Shal <marfey@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef tup_tupid_tree
#define tup_tupid_tree

#include "bsd/tree.h"
#include "tupid.h"

struct tupid_tree {
	RB_ENTRY(tupid_tree) linkage;
	tupid_t tupid;
};

struct tree_entry {
	struct tupid_tree tnode;
	int type;
};

RB_HEAD(tupid_entries, tupid_tree);
RB_PROTOTYPE(tupid_entries, tupid_tree, linkage, x);

struct tupid_tree *tupid_tree_search(struct tupid_entries *root, tupid_t tupid);
int tupid_tree_insert(struct tupid_entries *root, struct tupid_tree *data);
int tupid_tree_add(struct tupid_entries *root, tupid_t tupid);
int tupid_tree_add_dup(struct tupid_entries *root, tupid_t tupid);
void tupid_tree_remove(struct tupid_entries *root, tupid_t tupid);
static inline void tupid_tree_rm(struct tupid_entries *root, struct tupid_tree *tt)
{
	RB_REMOVE(tupid_entries, root, tt);
}
void free_tupid_tree(struct tupid_entries *root);
int tree_entry_add(struct tupid_entries *root, tupid_t tupid, int type, int *count);
void tree_entry_remove(struct tupid_entries *root, tupid_t tupid, int *count);

#endif
