/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2017  Mike Shal <marfey@gmail.com>
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

#include "thread_tree.h"

static int thread_tree_cmp(struct thread_tree *tt1, struct thread_tree *tt2)
{
	return tt1->id - tt2->id;
}

RB_GENERATE(thread_entries, thread_tree, linkage, thread_tree_cmp);

struct thread_tree *thread_tree_search(struct thread_root *troot, int id)
{
	struct thread_tree tt = {
		.id = id,
	};
	struct thread_tree *ret = NULL;

	pthread_mutex_lock(&troot->lock);
	ret = RB_FIND(thread_entries, &troot->root, &tt);
	pthread_mutex_unlock(&troot->lock);
	return ret;
}

int thread_tree_insert(struct thread_root *troot, struct thread_tree *data)
{
	int rc = 0;
	pthread_mutex_lock(&troot->lock);
	if(RB_INSERT(thread_entries, &troot->root, data) != NULL)
		rc = -1;
	pthread_cond_signal(&troot->cond);
	pthread_mutex_unlock(&troot->lock);
	return rc;
}

void thread_tree_rm(struct thread_root *troot, struct thread_tree *data)
{
	pthread_mutex_lock(&troot->lock);
	RB_REMOVE(thread_entries, &troot->root, data);
	pthread_mutex_unlock(&troot->lock);
}
