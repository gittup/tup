/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2021  Mike Shal <marfey@gmail.com>
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

#ifndef tup_thread_tree
#define tup_thread_tree

/* This is similar to the basic tupid_tree structure, except it uses an int
 * instead of a tupid, and has a mutex for automatic thread-safe operations.
 */

#include "bsd/tree.h"
#include <pthread.h>

struct thread_tree {
	RB_ENTRY(thread_tree) linkage;
	int id;
};

RB_HEAD(thread_entries, thread_tree);
RB_PROTOTYPE(thread_entries, thread_tree, linkage, x);

struct thread_root {
	struct thread_entries root;
	pthread_mutex_t lock;
	pthread_cond_t cond;
};
#define THREAD_ROOT_INITIALIZER {{NULL}, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER}

struct thread_tree *thread_tree_search(struct thread_root *troot, int id);
int thread_tree_insert(struct thread_root *troot, struct thread_tree *data);
void thread_tree_rm(struct thread_root *troot, struct thread_tree *data);

#endif
