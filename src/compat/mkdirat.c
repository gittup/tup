#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <string.h>

#ifdef _WIN32
#define mkdir(a,b) mkdir(a)
#endif

int mkdirat(int dirfd, const char *pathname, mode_t mode)
{
	char fullpath[MAXPATHLEN];

	fcntl(dirfd, F_GETPATH, fullpath);
	strlcat(fullpath, "/", MAXPATHLEN);
	strlcat(fullpath, pathname, MAXPATHLEN);

	return mkdir(pathname, mode);
}
