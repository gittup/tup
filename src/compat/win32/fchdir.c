#include <stdio.h>
#include <errno.h>
#include "dirpath.h"

int fchdir(int fd)
{
	const char *path;

	path = win32_get_dirpath(fd);
	if(path) {
		return chdir(path);
	}
	errno = EBADF;
	return -1;
}
