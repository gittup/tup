#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/param.h>
#include <string.h>

int fstatat(int dirfd, const char *pathname, struct stat *buf, int flags)
{
	char fullpath[MAXPATHLEN];

	fcntl(dirfd, F_GETPATH, fullpath);
	strlcat(fullpath, "/", MAXPATHLEN);
	strlcat(fullpath, pathname, MAXPATHLEN);

	if(flags & AT_SYMLINK_NOFOLLOW) {
		return lstat(fullpath, buf);
	} else {
		return stat(fullpath, buf);
	}
}
