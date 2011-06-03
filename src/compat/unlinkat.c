#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/param.h>
#include <string.h>

int unlinkat(int dirfd, const char *pathname, int flags)
{
	char fullpath[MAXPATHLEN];

	if(flags != 0) {
		fprintf(stderr, "tup compat unlinkat error: flags=%i not supported\n", flags);
		return -1;
	}

	fcntl(dirfd, F_GETPATH, fullpath);
	strlcat(fullpath, "/", MAXPATHLEN);
	strlcat(fullpath, pathname, MAXPATHLEN);

	return unlink(pathname);
}
