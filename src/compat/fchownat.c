#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "dir_mutex.h"

int fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags)
{
	int rc;
	int cwd;

	pthread_mutex_lock(&dir_mutex);

	cwd = open(".", O_RDONLY);
	if(fchdir(dirfd) < 0) {
		perror("fchdir");
		goto err_close;
	}
	if(flags & AT_SYMLINK_NOFOLLOW) {
		rc = lchown(pathname, owner, group);
	} else {
		rc = chown(pathname, owner, group);
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
