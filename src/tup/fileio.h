#ifndef tup_fileio_h
#define tup_fileio_h

#include "tupid.h"

struct db_node;

tupid_t create_name_file(tupid_t dt, const char *file);
tupid_t create_command_file(tupid_t dt, const char *cmd);
tupid_t create_varsed_file(tupid_t dt, const char *cmd);
tupid_t create_dir_file(tupid_t dt, const char *path);
tupid_t create_var_file(const char *var, const char *value);
int tup_file_mod(tupid_t dt, const char *file, int flags);
int tup_file_del(tupid_t tupid, tupid_t dt, int type);
int tup_pathname_mod(const char *path, int flags);
tupid_t get_dbn(const char *path, struct db_node *dbn);
tupid_t find_dir_tupid(const char *dir);
tupid_t find_dir_tupid_dt(tupid_t dt, const char *dir, const char **last);

int delete_name_file(tupid_t tupid);
int delete_dir_file(tupid_t tupid);

int canonicalize(const char *path, char *out, int len, int *lastslash);
int canonicalize2(const char *path, const char *file, char *out, int len,
		  int *lastslash, const char *subdir);

/** Canonicalizes a path name. Changes instances of "//" and "/./" to "/",
 * changes "foo/../bar" to "bar", and removes trailing slashes.
 *
 * The sz parameter is the size of the string buffer (including
 * nul-terminator).  The return value is the size of the shortened string (<=
 * sz), also including the nul-terminator.
 *
 * If not NULL, lastslash will be set to the index of the last '/' character
 * in the string. If there is no '/' in the string, lastslash will be set to
 * -1.
 */
int canonicalize_string(char *str, int sz, int *lastslash);

#endif
