#ifndef tup_compat_h
#define tup_compat_h

#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

int compat_init(void);
void compat_lock_enable(void);
void compat_lock_disable(void);

#ifdef _WIN32
#define is_path_sep(str) ((str)[0] == '/' || (str)[0] == '\\' || (str)[0] == ':' || ((str)[0] != '\0' && (str)[1] == ':'))
#define PATH_SEP '\\'
#define PATH_SEP_STR "\\"
#define SQL_NAME_COLLATION " collate nocase"
#define name_cmp stricmp
#define name_cmp_n strnicmp
/* Windows uses mtime, since ctime there is the creation time, not change time */
#define MTIME st_mtime
#else
#define is_path_sep(ch) ((ch)[0] == '/')
#define PATH_SEP '/'
#define PATH_SEP_STR "/"
#define SQL_NAME_COLLATION ""
#define name_cmp strcmp
#define name_cmp_n strncmp
/* Use ctime on other platforms, since chmod will affect ctime, but not mtime.
 * Also on Linux, ctime will be updated when a file is renamed (t6058).
 */
#define MTIME st_ctime
#endif

#endif
