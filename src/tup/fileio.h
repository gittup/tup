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

/** Creates a link. This link goes in the object directory of 'a' and links to
 * the .name file of 'b'
 */
int create_link(const tupid_t a, const tupid_t b);

int create_command_link(const tupid_t a, const tupid_t b);
int delete_link(const tupid_t a, const tupid_t b);

int create_tup_file(const char *tup, const char *path);
int create_tup_file_tupid(const char *tup, const tupid_t tupid);
int create_name_file(const char *path);
int create_command_file(const char *cmd);
int create_dir_file(const char *path);
int recreate_cmd_file(const tupid_t tupid);
int delete_tup_file(const char *tup, const tupid_t tupid);
int move_tup_file_if_exists(const char *tupsrc, const char *tupdst, const tupid_t tupid);
int num_dependencies(const tupid_t tupid);

/** Delete all memory of the file from .tup/object (except dangling refs). Also
 * removes the actual file, if it exists.
 *
 * Note: *not* thread safe.
 */
int delete_name_file(const tupid_t tupid);
int canonicalize(const char *path, char *out, int len);
int canonicalize2(const char *path, const char *file, char *out, int len);
void canonicalize_string(char *str, int len);

#endif
