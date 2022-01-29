/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2020-2022  Mike Shal <marfey@gmail.com>
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

#ifndef tup_tent_tree
#define tup_tent_tree

#include "bsd/tree.h"
#include "tupid.h"
struct tup_entry;

struct tent_tree {
	RB_ENTRY(tent_tree) linkage;
	struct tup_entry *tent;
};

struct tent_entries {
	struct tent_tree *rbh_root;
	int count;
};
RB_PROTOTYPE(tent_entries, tent_tree, linkage, x);

#define TENT_ENTRIES_INITIALIZER {NULL, 0}

void tent_tree_init(struct tent_entries *root);
int tent_tree_add(struct tent_entries *root, struct tup_entry *tent);
int tent_tree_add_dup(struct tent_entries *root, struct tup_entry *tent);
struct tent_tree *tent_tree_search(struct tent_entries *root, struct tup_entry *tent);
struct tent_tree *tent_tree_search_tupid(struct tent_entries *root, tupid_t tupid);
int tent_tree_copy(struct tent_entries *dest, struct tent_entries *src);
void tent_tree_remove(struct tent_entries *root, struct tup_entry *tent);
void tent_tree_rm(struct tent_entries *root, struct tent_tree *tt);
void free_tent_tree(struct tent_entries *root);

#endif
