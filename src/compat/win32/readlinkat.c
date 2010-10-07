#include <errno.h>

int readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz)
{
	if(dirfd) {}
	if(pathname) {}
	if(buf) {}
	if(bufsiz) {}
	errno = ENOSYS;
	return -1;
}
