/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2020-2021  Mike Shal <marfey@gmail.com>
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

#include "tupid_list.h"
#include "mempool.h"
#include <stdio.h>

static _Thread_local struct mempool pool = MEMPOOL_INITIALIZER(struct tupid_list);

int tupid_list_add_tail(struct tupid_list_head *head, tupid_t tupid)
{
	struct tupid_list *tlist;

	tlist = mempool_alloc(&pool);
	if(!tlist) {
		return -1;
	}
	tlist->tupid = tupid;
	TAILQ_INSERT_TAIL(head, tlist, list);
	return 0;
}

void tupid_list_delete(struct tupid_list_head *head, struct tupid_list *tlist)
{
	TAILQ_REMOVE(head, tlist, list);
	mempool_free(&pool, tlist);
}

void free_tupid_list(struct tupid_list_head *head)
{
	struct tupid_list *tlist;
	while(!TAILQ_EMPTY(head)) {
		tlist = TAILQ_FIRST(head);
		tupid_list_delete(head, tlist);
	}
}
