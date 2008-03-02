#include "fileio.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

int create_if_not_exist(const char *filename)
{
	struct stat buf;
	int rc;

	rc = stat(filename, &buf);
	if(rc == 0) {
		if(S_ISREG(buf.st_mode)) {
			return 0;
		} else {
			fprintf(stderr, "Error: '%s' exists and is not a "
				"regular file.\n", filename);
			return -1;
		}
	}

	rc = creat(filename, 0666);
	if(rc < 0) {
		perror(filename);
		return -1;
	}
	close(rc);
	return 0;
}
