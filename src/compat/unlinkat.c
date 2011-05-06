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

	pthread_mutex_lock(&dir_mutex);

	cwd = open(".", O_RDONLY);
	if(fchdir(dirfd) < 0) {
		close(cwd);
		perror("fchdir");
		goto err_unlock;
	}
	rc = unlink(pathname);
	fchdir(cwd);
	close(cwd);
	pthread_mutex_unlock(&dir_mutex);
	return rc;

err_unlock:
	pthread_mutex_unlock(&dir_mutex);
	return -1;
}
