/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2009-2023  Mike Shal <marfey@gmail.com>
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

#ifndef tup_bin_h
#define tup_bin_h

#include "bsd/queue.h"

struct tup_entry;

struct bin_entry {
	TAILQ_ENTRY(bin_entry) list;
	char *path;
	int len;
	struct tup_entry *tent;
};
TAILQ_HEAD(bin_entry_head, bin_entry);

struct bin {
	LIST_ENTRY(bin) list;
	char *name;
	struct bin_entry_head entries;
};
LIST_HEAD(bin_head, bin);

void bin_list_del(struct bin_head *head);
struct bin *bin_add(const char *name, struct bin_head *head);
struct bin *bin_find(const char *name, struct bin_head *head);
int bin_add_entry(struct bin *b, const char *path, int len,
		  struct tup_entry *tent);

#endif
