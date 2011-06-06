#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "dir_mutex.h"

int unlinkat(int dirfd, const char *pathname, int flags)
{
	int rc;
	int cwd;

	if(flags != 0) {
		fprintf(stderr, "tup compat unlinkat error: flags=%i not supported\n", flags);
		return -1;
	}

	cwd = dir_mutex_lock(dirfd);
	rc = unlink(pathname);
	dir_mutex_unlock(cwd);
	return rc;
}
