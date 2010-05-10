#ifndef tup_compat_h
#define tup_compat_h

#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

int compat_init(void);

#ifdef _WIN32
#define is_path_sep(str) ((str)[0] == '/' || (str)[0] == '\\' || (str)[0] == ':' || ((str)[0] != '\0' && (str)[1] == ':'))
#define is_path_abs(str) (is_path_sep(str) || ((str)[0] == '\0' && (str)[1] == ':'))
#define path_sep '\\'
#define path_sep_str "\\"
#else
#define is_path_sep(ch) ((ch)[0] == '/')
#define is_path_abs(str) is_path_sep(str)
#define path_sep '/'
#define path_sep_str "/"
#endif

#endif
