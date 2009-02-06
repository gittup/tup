#ifndef dircache_h
#define dircache_h

#include "list.h"
#include "tupid.h"

struct dircache {
	struct list_head list;
	int wd;
	tupid_t dt;
	char *path;
};

struct memdb;

/** Adds the given wd -> path relationship to the dircache. This assumes
 * ownership of the memory pointed to by path, which must have been allocated
 * by malloc() or equivalent.
 */
void dircache_add(struct memdb *m, int wd, char *path, tupid_t dt);

/** Returns the path given when wd was added to the dircache, or NULL if not
 * found.
 */
struct dircache *dircache_lookup(struct memdb *m, int wd);

/** Deletes the wd from the dircache. */
void dircache_del(struct memdb *m, struct dircache *dc);

#endif
