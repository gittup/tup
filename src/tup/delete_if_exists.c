#include "fileio.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

int delete_if_exists(const char *path)
{
	struct stat buf;

	if(stat(path, &buf) < 0)
		return 0;
	if(S_ISREG(buf.st_mode)) {
		if(unlink(path) < 0) {
			perror(path);
			return -1;
		}
	}
	return 0;
}
