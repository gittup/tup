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

#include "mempool.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

static struct mementry_head head = SLIST_HEAD_INITIALIZER(head);
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *mempool_alloc(struct mempool *pool)
{
	struct mementry *ret;
	struct mementry *block;

	/* If an entry is available on the free list, grab that first */
	ret = SLIST_FIRST(&pool->free_list);
	if(ret) {
		SLIST_REMOVE_HEAD(&pool->free_list, list);
		return ret;
	}

	if(!pool->free_count) {
		/* If we don't have space in the pool to make a new item,
		 * allocate a new block. The new block uses an SLIST_ENTRY to
		 * get linked into the global mementry_head in this file so
		 * that we can free all memory when using valgrind. Otherwise,
		 * memory allocated here is not freed until the program quits.
		 */
		if(pool->item_size < sizeof(struct mementry)) {
			fprintf(stderr, "tup internal error: mempool item size too small: %i\n", pool->item_size);
			return NULL;
		}

		block = malloc(pool->next_alloc_size);
		if(!block) {
			perror("malloc");
			return NULL;
		}
		pthread_mutex_lock(&lock);
		SLIST_INSERT_HEAD(&head, block, list);
		pthread_mutex_unlock(&lock);

		/* The available memory for items starts after the global
		 * SLIST_ENTRY. Note that we don't add all new items to the
		 * free list; both the free list itself and the free_count
		 * items pointed to by mem are considered free. Items from the
		 * free list are used first. The allocation size is doubled for
		 * the next time we run out of items.
		 */
		pool->mem = (char*)block + sizeof(*block);
		pool->free_count = (pool->next_alloc_size - sizeof(*block)) / pool->item_size;
		pool->next_alloc_size *= 2;
	}

	/* Since we didn't have any items available from the free_list, use the
	 * next item from the pool's memory.
	 */
	if(((uintptr_t)pool->mem & (pool->alignment-1)) != 0) {
		fprintf(stderr, "tup internal error: memory address in mempool (%p) not aligned to %u bytes.\n", pool->mem, pool->alignment);
		return NULL;
	}
	ret = (void*)pool->mem;
	pool->mem += pool->item_size;
	pool->free_count--;
	return ret;
}

void mempool_free(struct mempool *pool, void *item)
{
	if(item) {
		struct mementry *entry = item;
		SLIST_INSERT_HEAD(&pool->free_list, entry, list);
	}
}

void mempool_clear(void)
{
	pthread_mutex_lock(&lock);
	while(!SLIST_EMPTY(&head)) {
		struct mementry *entry = SLIST_FIRST(&head);
		SLIST_REMOVE_HEAD(&head, list);
		free(entry);
	}
	pthread_mutex_unlock(&lock);
}
