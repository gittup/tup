#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include "dir_mutex.h"

int openat(int dirfd, const char *pathname, int flags, ...)
{
	int fd;
	mode_t mode = 0;

	dir_mutex_lock(dirfd);
	if(flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	}
	fd = open(pathname, flags, mode);
	dir_mutex_unlock();
	return fd;
}
