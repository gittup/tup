#ifndef sha1dep_h
#define sha1dep_h

#include "tupid.h"

/** Write the dependency relation "file: depends_on" using the sha1 hashes
 * in the filesystem.
 *
 * Note: *not* thread safe.
 */
int write_sha1dep(const tupid_t file, const tupid_t depends_on);

#endif
