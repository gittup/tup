#ifndef fileio_h
#define fileio_h

#include "tup/tupid.h"

/** Assuming the directory exists to hold the file, an empty file is created at
 * the given path if it doesn't already exist. Returns 0 on success, -1 on
 * failure.
 */
int create_if_not_exist(const char *filename);

/** Creates a hard link from src to dest, unless dest already exists */
int link_if_not_exist(const char *src, const char *dest);

/** Removes a file if it exists */
int remove_if_exists(const char *path);

/** Basically write(), but returns -1 if the write fails or if the length
 * returned doesn't equal the 'size' argument.
 *
 * The 'filename' parameter is only used for error messages.
 */
int write_all(int fd, const void *buf, int size, const char *filename);

/** Write the dependency relation "file: depends_on" using the sha1 hashes
 * in the filesystem.
 *
 * Note: *not* thread safe.
 */
int write_sha1dep(const tupid_t file, const tupid_t depends_on);

/** Make a directory hierarchy to support the given filename. For example,
 * if given a/b/c/foo.txt, this makes sure the directories a/, a/b/, and a/b/c/
 * exist - if not, it creates them.
 *
 * The data pointed to by 'filename' is temporarily modified, but left
 * unchanged upon.
 */
int mkdirhier(char *filename);

int create_tup_file(const char *path, const char *file, const char *tup,
		    int lock_fd);
int create_name_file(const char *file, int lock_fd);
int create_name_file2(const char *path, const char *file, int lock_fd);
int remove_tup_file(const char *tup, const tupid_t tupid);
int move_tup_file(const tupid_t tupid, const char *tupsrc, const char *tupdst);

#endif
