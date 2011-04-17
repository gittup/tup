#ifndef tup_thread_tree
#define tup_thread_tree

/* This is similar to the basic tupid_tree structure, except it uses an int
 * instead of a tupid, and has a mutex for automatic thread-safe operations.
 */

#include "linux/rbtree.h"
#include <pthread.h>

struct thread_root {
	struct rb_root root;
	pthread_mutex_t lock;
};
#define THREAD_ROOT_INITIALIZER {RB_ROOT, PTHREAD_MUTEX_INITIALIZER}

struct thread_tree {
	struct rb_node rbn;
	int id;
};

struct thread_tree *thread_tree_search(struct thread_root *troot, int id);
int thread_tree_insert(struct thread_root *troot, struct thread_tree *data);
void thread_tree_rm(struct thread_root *troot, struct thread_tree *data);

#endif
