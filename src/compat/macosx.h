#ifndef tup_compat_macosx_h
#define tup_compat_macosx_h

#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>

// MacOSX 10.6 does not have *at() functions
DIR *fdopendir(int fd);
int faccessat(int dirfd, const char *pathname, int mode, int flags);
int fchmodat(int dirfd, const char *pathname, mode_t mode, int flags);
int fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags);
int fstatat(int dirfd, const char *pathname, struct stat *buf, int flags);
int mkdirat(int dirfd, const char *pathname, mode_t mode);
int openat(int dirfd, const char *pathname, int flags, ...);
int readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz);
int renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath);
int symlinkat(const char *oldpath, int newdirfd, const char *newpath);
int unlinkat(int dirfd, const char *pathname, int flags);
int utimensat(int dirfd, const char *pathname, const struct timespec times[2], int flags);

#endif
