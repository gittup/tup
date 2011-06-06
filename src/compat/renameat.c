#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "dir_mutex.h"

int renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath)
{
	int rc;
	int cwd;

	if(olddirfd != newdirfd) {
		fprintf(stderr, "tup compat renameat error: olddirfd=%i but newdirfd=%i\n", olddirfd, newdirfd);
		return -1;
	}

	cwd = dir_mutex_lock(olddirfd);
	rc = rename(oldpath, newpath);
	dir_mutex_unlock(cwd);
	return rc;
}
