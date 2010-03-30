#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "dir_mutex.h"
#include "tup/config.h"

int fstatat(int dirfd, const char *pathname, struct stat *buf, int flags)
{
	int rc;

	if(pthread_mutex_lock(&dir_mutex) < 0) {
		perror("pthread_mutex_lock");
		return -1;
	}

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
