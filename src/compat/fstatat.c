#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "dir_mutex.h"

int fstatat(int dirfd, const char *pathname, struct stat *buf, int flags)
{
	int rc;
	int cwd;

	pthread_mutex_lock(&dir_mutex);

	cwd = open(".", O_RDONLY);
	if(fchdir(dirfd) < 0) {
		perror("fchdir");
		close(cwd);
		goto err_unlock;
	}
	if(flags & AT_SYMLINK_NOFOLLOW) {
		rc = lstat(pathname, buf);
	} else {
		rc = stat(pathname, buf);
	}
	fchdir(cwd);
	close(cwd);
	pthread_mutex_unlock(&dir_mutex);
	return rc;

err_unlock:
	pthread_mutex_unlock(&dir_mutex);
	return -1;
}
