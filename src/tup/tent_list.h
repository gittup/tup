/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2020-2023  Mike Shal <marfey@gmail.com>
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

#ifndef tup_tent_list
#define tup_tent_list

#include "bsd/queue.h"
struct tup_entry;

struct tent_list {
	TAILQ_ENTRY(tent_list) list;
	struct tup_entry *tent;
};
TAILQ_HEAD(tent_list_head, tent_list);

#define tent_list_init(head) TAILQ_INIT(head)
#define tent_list_empty(head) TAILQ_EMPTY(head)
#define tent_list_first(head) TAILQ_FIRST(head)
#define tent_list_foreach(tlist, head) TAILQ_FOREACH(tlist, head, list)
int tent_list_add_head(struct tent_list_head *head, struct tup_entry *tent);
int tent_list_add_tail(struct tent_list_head *head, struct tup_entry *tent);
void tent_list_delete(struct tent_list_head *head, struct tent_list *tlist);
void free_tent_list(struct tent_list_head *head);

#endif
