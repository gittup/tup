#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include "dir_mutex.h"

int fchmodat(int dirfd, const char *pathname, mode_t mode, int flags)
{
	int rc;

	dir_mutex_lock(dirfd);
	if(flags & AT_SYMLINK_NOFOLLOW) {
		fprintf(stderr, "tup error: fchmodat(AT_SYMLINK_NOFOLLOW) not supported in compat library.\n");
		rc = -1;
		errno = ENOSYS;
	} else {
		rc = chmod(pathname, mode);
	}
	dir_mutex_unlock();
	return rc;
}
