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

#ifndef tup_tupid_list
#define tup_tupid_list

#include "bsd/queue.h"
#include "tupid.h"

struct tupid_list {
	TAILQ_ENTRY(tupid_list) list;
	tupid_t tupid;
};
TAILQ_HEAD(tupid_list_head, tupid_list);

#define tupid_list_init(head) TAILQ_INIT(head)
#define tupid_list_empty(head) TAILQ_EMPTY(head)
#define tupid_list_first(head) TAILQ_FIRST(head)
#define tupid_list_foreach(tlist, head) TAILQ_FOREACH(tlist, head, list)
int tupid_list_add_tail(struct tupid_list_head *head, tupid_t tupid);
void tupid_list_delete(struct tupid_list_head *head, struct tupid_list *tlist);
void free_tupid_list(struct tupid_list_head *head);

#endif
