#ifndef dircache_h
#define dircache_h

#include "linux/list.h"
#include "tupid.h"

struct dircache {
	struct list_head list;
	int wd;
	tupid_t dt;
};

struct memdb;

/** Adds the given wd -> dt relationship to the dircache.  */
void dircache_add(struct memdb *m, int wd, tupid_t dt);

/** Returns the dircache given when wd was added to the dircache, or NULL if
 * not found.
 */
struct dircache *dircache_lookup(struct memdb *m, int wd);

/** Deletes dc from the dircache. */
void dircache_del(struct memdb *m, struct dircache *dc);

#endif
