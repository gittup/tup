#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "dir_mutex.h"

int fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags)
{
	int rc;

	dir_mutex_lock(dirfd);
	if(flags & AT_SYMLINK_NOFOLLOW) {
		rc = lchown(pathname, owner, group);
	} else {
		rc = chown(pathname, owner, group);
	}
	dir_mutex_unlock();
	return rc;
}
