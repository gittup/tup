/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2020  Mike Shal <marfey@gmail.com>
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

#include "tent_list.h"
#include <stdio.h>
#include <stdlib.h>

int tent_list_add_head(struct tent_list_head *head, struct tup_entry *tent)
{
	struct tent_list *tlist;

	tlist = malloc(sizeof *tlist);
	if(!tlist) {
		perror("malloc");
		return -1;
	}
	tlist->tent = tent;
	TAILQ_INSERT_HEAD(head, tlist, list);
	return 0;
}

int tent_list_add_tail(struct tent_list_head *head, struct tup_entry *tent)
{
	struct tent_list *tlist;

	tlist = malloc(sizeof *tlist);
	if(!tlist) {
		perror("malloc");
		return -1;
	}
	tlist->tent = tent;
	TAILQ_INSERT_TAIL(head, tlist, list);
	return 0;
}

void tent_list_delete(struct tent_list_head *head, struct tent_list *tlist)
{
	TAILQ_REMOVE(head, tlist, list);
	free(tlist);
}

void free_tent_list(struct tent_list_head *head)
{
	struct tent_list *tlist;
	while(!TAILQ_EMPTY(head)) {
		tlist = TAILQ_FIRST(head);
		tent_list_delete(head, tlist);
	}
}
