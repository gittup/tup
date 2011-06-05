#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "dir_mutex.h"

int fchmodat(int dirfd, const char *pathname, mode_t mode, int flags)
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
		fprintf(stderr, "tup error: fchmodat(AT_SYMLINK_NOFOLLOW) not supported in compat library.\n");
		rc = -1;
	} else {
		rc = chmod(pathname, mode);
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
