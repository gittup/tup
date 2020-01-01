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

#ifndef tup_string_tree_h
#define tup_string_tree_h

#include "bsd/tree.h"

struct string_tree {
	RB_ENTRY(string_tree) linkage;
	char *s;
	int len;
};

RB_HEAD(string_entries, string_tree);
RB_PROTOTYPE(string_entries, string_tree, linkage, x);

int string_tree_insert(struct string_entries *root, struct string_tree *st);
struct string_tree *string_tree_search(struct string_entries *root, const char *s,
				       int len);

/* _add is like _insert, but also malloc()s and copies 's' into 'st->s'. _free
 * just free()s st->s and calls _rm.
 */
int string_tree_add(struct string_entries *root, struct string_tree *st, const char *s);
void string_tree_remove(struct string_entries *root, struct string_tree *st);
void free_string_tree(struct string_entries *root);
static inline void string_tree_rm(struct string_entries *root, struct string_tree *st)
{
	RB_REMOVE(string_entries, root, st);
}

#endif
