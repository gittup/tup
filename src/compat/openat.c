#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include "dir_mutex.h"

int openat(int dirfd, const char *pathname, int flags, ...)
{
	int fd;
	mode_t mode = 0;
	int cwd;

	pthread_mutex_lock(&dir_mutex);

	cwd = open(".", O_RDONLY);
	if(fchdir(dirfd) < 0) {
		close(cwd);
		perror("fchdir");
		goto err_unlock;
	}
	if(flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	}
	fd = open(pathname, flags, mode);
	fchdir(cwd);
	close(cwd);
	pthread_mutex_unlock(&dir_mutex);
	return fd;

err_unlock:
	pthread_mutex_unlock(&dir_mutex);
	return -1;
}
