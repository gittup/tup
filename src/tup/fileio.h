#ifndef tup_fileio_h
#define tup_fileio_h

#include "tup/tupid.h"

/** Assuming the directory exists to hold the file, an empty file is created at
 * the given path if it doesn't already exist. Returns 0 on success, -1 on
 * failure.
 */
int create_if_not_exist(const char *filename);

/** Removes a file if it exists */
int delete_if_exists(const char *path);

/** Basically write(), but returns -1 if the write fails or if the length
 * returned doesn't equal the 'size' argument.
 *
 * The 'filename' parameter is only used for error messages.
 */
int write_all(int fd, const void *buf, int size, const char *filename);

/** Creates a primary link (responsible for detecting deletions). This link
 * goes in the object directory of 'a' and links to the .name file of 'b'
 */
int create_primary_link(const tupid_t a, const tupid_t b);

/** Creates a secondary link, which allows for updates but does nothing for
 * deletions. This link goes in the object directory of 'a' and links to the
 * .secondary file of 'b'
 */
int create_secondary_link(const tupid_t a, const tupid_t b);

/** Make a directory hierarchy to support the given filename. For example,
 * if given a/b/c/foo.txt, this makes sure the directories a/, a/b/, and a/b/c/
 * exist - if not, it creates them.
 *
 * The data pointed to by 'filename' is temporarily modified, but left
 * unchanged upon.
 */
int mkdirhier(char *filename);

int create_tup_file(const char *tup, const char *path, const char *file);
int create_tup_file_tupid(const char *tup, const tupid_t tupid);
int create_name_file(const char *file);
int create_name_file2(const char *path, const char *file);
int recreate_name_file(const tupid_t tupid);
int delete_tup_file(const char *tup, const tupid_t tupid);
int move_tup_file(const char *tupsrc, const char *tupdst, const tupid_t tupid);
int num_dependencies(const tupid_t tupid);
int delete_name_file(const tupid_t tupid);
int canonicalize(const char *path, const char *file, char *out, int len);

#endif
