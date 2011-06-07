#include <stdio.h>
#include <sys/param.h>
#include <sys/fcntl.h>
#include <dirent.h>

DIR *fdopendir(int fd)
{
#ifdef __APPLE__
	char fullpath[MAXPATHLEN];
	DIR *d;

	if(fcntl(fd, F_GETPATH, fullpath) < 0) {
		perror("fcntl");
		fprintf(stderr, "tup error: Unable to convert file descriptor back to pathname in fdopendir() compat library.\n");
		return NULL;
	}

	d = opendir(fullpath);
	return d;
#else
#error Unsupported platform in fdopendir.c compat library
#endif
}
