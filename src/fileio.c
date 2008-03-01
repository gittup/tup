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
		perror("creat");
		return -1;
	}
	close(rc);
	return 0;
}

int link_if_not_exist(const char *src, const char *dest)
{
	struct stat buf;
	int rc;

	rc = stat(dest, &buf);
	if(rc == 0) {
		if(S_ISREG(buf.st_mode)) {
			return 0;
		} else {
			fprintf(stderr, "Error: '%s' exists and is not a "
				"regular file.\n", dest);
			return -1;
		}
	}

	rc = link(src, dest);
	if(rc < 0) {
		perror(dest);
		return -1;
	}
	return 0;
}

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
