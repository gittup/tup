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

	cwd = dir_mutex_lock(dirfd);
	if(flags & AT_SYMLINK_NOFOLLOW) {
		rc = lstat(pathname, buf);
	} else {
		rc = stat(pathname, buf);
	}
	dir_mutex_unlock(cwd);
	return rc;
}
