/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2009-2015  Mike Shal <marfey@gmail.com>
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

#include "string_tree.h"
#include "compat.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int string_tree_cmp(struct string_tree *st1, struct string_tree *st2)
{
	int result;
	result = name_cmp_n(st1->s, st2->s, st1->len);
	if(result == 0)
		result = st1->len - st2->len;
	return result;
}

RB_GENERATE(string_entries, string_tree, linkage, string_tree_cmp);

int string_tree_insert(struct string_entries *root, struct string_tree *st)
{
	if(RB_INSERT(string_entries, root, st) != NULL) {
		return -1;
	}
	return 0;
}

struct string_tree *string_tree_search(struct string_entries *root, const char *s,
				       int len)
{
	struct string_tree tmp;
	char buf[len+1];
	memcpy(buf, s, len);
	buf[len] = 0;
	tmp.s = buf;
	tmp.len = len;
	return RB_FIND(string_entries, root, &tmp);
}

int string_tree_add(struct string_entries *root, struct string_tree *st, const char *s)
{
	st->len = strlen(s);
	st->s = malloc(st->len + 1);
	if(!st->s) {
		perror("malloc");
		return -1;
	}

	memcpy(st->s, s, st->len);
	st->s[st->len] = 0;

	if(RB_INSERT(string_entries, root, st) != NULL) {
		free(st->s);
		return -1;
	}
	return 0;
}

void string_tree_free(struct string_entries *root, struct string_tree *st)
{
	RB_REMOVE(string_entries, root, st);
	free(st->s);
}
