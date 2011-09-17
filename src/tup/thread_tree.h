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
};
#define THREAD_ROOT_INITIALIZER {{NULL}, PTHREAD_MUTEX_INITIALIZER}

struct thread_tree *thread_tree_search(struct thread_root *troot, int id);
int thread_tree_insert(struct thread_root *troot, struct thread_tree *data);
void thread_tree_rm(struct thread_root *troot, struct thread_tree *data);

#endif
