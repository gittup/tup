#ifndef tup_fileio_h
#define tup_fileio_h

#include "tup/tupid.h"

/** Assuming the directory exists to hold the file, an empty file is created at
 * the given path if it doesn't already exist. Returns 0 on success, -1 on
 * failure.
 */
int create_if_not_exist(const char *path);

/** Removes a file if it exists */
int delete_if_exists(const char *path);

/** Basically write(), but returns -1 if the write fails or if the length
 * returned doesn't equal the 'size' argument.
 *
 * The 'filename' parameter is only used for error messages.
 */
int write_all(int fd, const void *buf, int size, const char *filename);

/** Creates a link, a -> b.  */
int create_link(const new_tupid_t a, const new_tupid_t b);
int find_link(const char *from, const char *to);

int delete_link(const tupid_t a, const tupid_t b);

int create_tup_file(const char *tup, const char *path);
int create_tup_file_tupid(const char *tup, const tupid_t tupid);
new_tupid_t create_name_file(const char *path);
new_tupid_t create_command_file(const char *cmd);
new_tupid_t create_dir_file(const char *path);
int recreate_cmd_file(const tupid_t tupid);
int delete_tup_file(const char *tup, const tupid_t tupid);
int move_tup_file_if_exists(const char *tupsrc, const char *tupdst, const tupid_t tupid);
int num_dependencies(new_tupid_t tupid);
int update_node_flags(const char *name, int flags);
new_tupid_t select_node(const char *name);

/** Delete all memory of the file from .tup/object (except dangling refs). Also
 * removes the actual file, if it exists.
 */
int delete_name_file(new_tupid_t tupid);

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
