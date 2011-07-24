#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "dir_mutex.h"

int faccessat(int dirfd, const char *pathname, int mode, int flags)
{
	int rc;

	if(flags) {/* No way to access() a symlink? */}

	dir_mutex_lock(dirfd);
	rc = access(pathname, mode);
	dir_mutex_unlock();
	return rc;
}
