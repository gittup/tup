#ifndef sha1dep_h
#define sha1dep_h

/** Write the dependency relation "file: depends_on" using the sha1 hashes
 * in the filesystem.
 *
 * Note: *not* thread safe.
 */
int write_sha1dep(const char *file, const char *depends_on);

#endif
