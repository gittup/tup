#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "dir_mutex.h"

int fstatat(int dirfd, const char *pathname, struct stat *buf, int flags)
{
	int rc;

	pthread_mutex_lock(&dir_mutex);

	if(fchdir(dirfd) < 0) {
		perror("fchdir");
		goto err_unlock;
	}
	if(flags & AT_SYMLINK_NOFOLLOW) {
		rc = lstat(pathname, buf);
	} else {
		rc = stat(pathname, buf);
	}
	pthread_mutex_unlock(&dir_mutex);
	return rc;

err_unlock:
	pthread_mutex_unlock(&dir_mutex);
	return -1;
}
