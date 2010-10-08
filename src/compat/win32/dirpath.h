#ifndef tup_win32_dirpath_h
#define tup_win32_dirpath_h

const char *win32_get_dirpath(int dfd);
int win32_add_dirpath(const char *path);
int win32_rm_dirpath(int dfd);
int win32_dup(int oldfd);

#endif
