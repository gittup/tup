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
	pthread_mutex_unlock(&troot->lock);
	return rc;
}

void thread_tree_rm(struct thread_root *troot, struct thread_tree *data)
{
	pthread_mutex_lock(&troot->lock);
	RB_REMOVE(thread_entries, &troot->root, data);
	pthread_mutex_unlock(&troot->lock);
}
