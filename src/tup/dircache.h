#ifndef dircache_h
#define dircache_h

#include "tupid_tree.h"
#include "linux/rbtree.h"

/* The tupid in wd_node isn't a real tupid, but rather the watch descriptor
 * for inotify. The tupid in dt_node is the directory's tupid, like usual.
 */
struct dircache {
	struct tupid_tree wd_node;
	struct tupid_tree dt_node;
};

/* Contains the both trees for looking up dircache entries with either the
 * watch descriptor or the dir tupid.
 */
struct dircache_root {
	struct rb_root wd_tree;
	struct rb_root dt_tree;
};

/* Initializes the dircache_root structure */
void dircache_init(struct dircache_root *droot);

/* Adds the given wd -> dt relationship to the dircache.  */
void dircache_add(struct dircache_root *droot, int wd, tupid_t dt);

/* Returns the dircache given when wd was added to the dircache, or NULL if not
 * found.
 */
struct dircache *dircache_lookup_wd(struct dircache_root *droot, int wd);

/* Same as dircache_lookup_wd, only the dir tupid is used instead of the watch
 * descriptor.
 */
struct dircache *dircache_lookup_dt(struct dircache_root *droot, tupid_t dt);

/* Deletes dc from the dircache. */
void dircache_del(struct dircache_root *droot, struct dircache *dc);

#endif
