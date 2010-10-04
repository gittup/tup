#include <stddef.h> /* get size_t */
#include <fcntl.h> /* get mode_t */
#define AT_SYMLINK_NOFOLLOW 0x100

struct stat;

int fchdir(int fd);
int fstatat(int dirfd, const char *pathname, struct stat *buf, int flags);
int mkstemp(char *template);
int openat(int dirfd, const char *pathname, int flags, ...);
int unlinkat(int dirfd, const char *pathname, int flags);
int readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz);
int mkdirat(int dirfd, const char *pathname, mode_t mode);
