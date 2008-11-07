#ifndef dircache_h
#define dircache_h

#include "tup/list.h"

struct dircache {
	struct list_head list;
	int wd;
	char *path;
};

/** Adds the given wd -> path relationship to the dircache. This assumes
 * ownership of the memory pointed to by path, which must have been allocated
 * by malloc() or equivalent.
 */
void dircache_add(int wd, char *path);

/** Returns the path given when wd was added to the dircache, or NULL if not
 * found.
 */
struct dircache *dircache_lookup(int wd);

/** Deletes the wd from the dircache. */
void dircache_del(struct dircache *dc);

#endif
