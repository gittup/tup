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

#ifndef tup_mempool
#define tup_mempool

#include "bsd/queue.h"

struct mementry {
	SLIST_ENTRY(mementry) list;
};
SLIST_HEAD(mementry_head, mementry);

struct mempool {
	struct mementry_head free_list;
	unsigned int item_size;
	unsigned int next_alloc_size;
	unsigned int alignment;
	int free_count;
	char *mem;
};
TAILQ_HEAD(mempool_head, mempool);

#define ALLOC_BLOCK_SIZE 4096
#define MEMPOOL_INITIALIZER(a) {.free_list = {NULL}, .item_size=sizeof(a), .next_alloc_size=ALLOC_BLOCK_SIZE, .alignment=_Alignof(a), .free_count=0, .mem=NULL}

void *mempool_alloc(struct mempool *pool);
void mempool_free(struct mempool *pool, void *item);
void mempool_clear(void);

#endif
