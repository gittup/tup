#ifndef tup_compat_macosx_h
#define tup_compat_macosx_h

#include <stdlib.h>
#include <sys/stat.h>

// MacOSX 10.6 does not have *at() functions
int openat(int dirfd, const char *pathname, int flags, ...);
int readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz);
int fstatat(int dirfd, const char *pathname, struct stat *buf, int flags);
int unlinkat(int dirfd, const char *pathname, int flags);
int mkdirat(int dirfd, const char *pathname, mode_t mode);

#endif
