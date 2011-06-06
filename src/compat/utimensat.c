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

	cwd = dir_mutex_lock(dirfd);
	if(flags & AT_SYMLINK_NOFOLLOW) {
		rc = lutimes(pathname, tv);
	} else {
		rc = utimes(pathname, tv);
	}
	dir_mutex_unlock(cwd);
	return rc;
}
