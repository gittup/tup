#include "fileio.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

int create_if_not_exist(const char *path)
{
	struct stat buf;
	int rc;

	rc = stat(path, &buf);
	if(rc == 0) {
		if(S_ISREG(buf.st_mode)) {
			return 0;
		} else {
			fprintf(stderr, "Error: '%s' exists and is not a "
				"regular file.\n", path);
			return -1;
		}
	}

	rc = creat(path, 0666);
	if(rc < 0) {
		perror(path);
		return -1;
	}
	close(rc);
	return 0;
}
