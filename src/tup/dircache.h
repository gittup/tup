#ifndef dircache_h
#define dircache_h

#include "tupid_tree.h"

/* The tupid in tnode is actually the watch descriptor from inotify, since
 * that is what we use in the search function.
 */
struct dircache {
	struct tupid_tree tnode;
	tupid_t dt;
};

struct rb_root;

/** Adds the given wd -> dt relationship to the dircache.  */
void dircache_add(struct rb_root *tree, int wd, tupid_t dt);

/** Returns the dircache given when wd was added to the dircache, or NULL if
 * not found.
 */
struct dircache *dircache_lookup(struct rb_root *tree, int wd);

/** Deletes dc from the dircache. */
void dircache_del(struct rb_root *tree, struct dircache *dc);

#endif
