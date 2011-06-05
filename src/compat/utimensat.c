#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "dir_mutex.h"

int utimensat(int dirfd, const char *pathname, const struct timespec times[2], int flags)
{
	int rc;
	int cwd;
	struct timeval tv[2];
	tv[0].tv_sec = times[0].tv_sec;
	tv[0].tv_usec = times[0].tv_nsec / 1000;
	tv[1].tv_sec = times[1].tv_sec;
	tv[1].tv_usec = times[1].tv_nsec / 1000;

	pthread_mutex_lock(&dir_mutex);

	cwd = open(".", O_RDONLY);
	if(fchdir(dirfd) < 0) {
		perror("fchdir");
		goto err_close;
	}
	if(flags & AT_SYMLINK_NOFOLLOW) {
		rc = lutimes(pathname, tv);
	} else {
		rc = utimes(pathname, tv);
	}
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
