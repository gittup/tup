#ifndef tup_fileio_h
#define tup_fileio_h

#include "tup/tupid.h"

/** Creates a link, a -> b.  */
int create_link(tupid_t a, tupid_t b);
int find_link(const char *from, const char *to);

tupid_t create_name_file(const char *path);
tupid_t create_command_file(const char *cmd);
tupid_t create_dir_file(const char *path);
int update_create_dir_for_file(char *name);
int num_dependencies(tupid_t tupid);

/** Delete all memory of the file from .tup/object (except dangling refs). Also
 * removes the actual file, if it exists.
 */
int delete_name_file(tupid_t tupid);

int canonicalize(const char *path, char *out, int len);
int canonicalize2(const char *path, const char *file, char *out, int len);

/** Canonicalizes a path name. Changes instances of "//" and "/./" to "/",
 * changes "foo/../bar" to "bar", and removes trailing slashes.
 *
 * The sz parameter is the size of the string buffer (including
 * nul-terminator).  The return value is the size of the shortened string (<=
 * sz), also including the nul-terminator.
 */
int canonicalize_string(char *str, int sz);

#endif
