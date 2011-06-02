#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/param.h>
#include <string.h>

int openat(int dirfd, const char *pathname, int flags, ...)
{
	char fullpath[MAXPATHLEN];
	mode_t mode = 0;

	fcntl(dirfd, F_GETPATH, fullpath);
	strlcat(fullpath, "/", MAXPATHLEN);
	strlcat(fullpath, pathname, MAXPATHLEN);

	if(flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	}
	return open(fullpath, flags, mode);
}
