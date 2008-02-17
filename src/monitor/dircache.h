#ifndef dircache_h
#define dircache_h

/** Adds the given wd -> path relationship to the dircache. This assumes
 * ownership of the memory pointed to by path, which must have been allocated
 * by malloc() or equivalent.
 */
void dircache_add(int wd, char *path);

/** Returns the path given when wd was added to the dircache, or NULL if not
 * found.
 */
const char *dircache_lookup(int wd);

/** Deletes the wd from the dircache. */
void dircache_del(int wd);

#endif
