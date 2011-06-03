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

	pthread_mutex_lock(&dir_mutex);

	cwd = open(".", O_RDONLY);
	if(fchdir(olddirfd) < 0) {
		perror("fchdir");
		goto err_close;
	}
	rc = rename(oldpath, newpath);
	if(fchdir(cwd) < 0) {
		perror("fchdir");
		goto err_close;
	}
	close(cwd);
	pthread_mutex_unlock(&dir_mutex);
	return rc;

err_close:
	close(cwd);
	pthread_mutex_unlock(&dir_mutex);
	return -1;
}
