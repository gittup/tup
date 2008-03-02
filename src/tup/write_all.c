#include "fileio.h"
#include <stdio.h>
#include <unistd.h>

int write_all(int fd, const void *buf, int size, const char *filename)
{
	int rc;
	rc = write(fd, buf, size);
	if(rc < 0) {
		perror("write");
		return -1;
	}
	if(rc != size) {
		fprintf(stderr, "Unable to write all %i bytes to %s\n",
			size, filename);
		return -1;
	}
	return 0;
}
