#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/param.h>
#include <string.h>

int readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz)
{
	char fullpath[MAXPATHLEN];

	fcntl(dirfd, F_GETPATH, fullpath);
	strlcat(fullpath, "/", MAXPATHLEN);
	strlcat(fullpath, pathname, MAXPATHLEN);

	return readlink(pathname, buf, bufsiz);
}
