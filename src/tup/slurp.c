#include "slurp.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

int slurp(const char *filename, struct buf *b)
{
	int fd;
	int rc;

	fd = open(filename, O_RDONLY);
	if(fd < 0) {
		perror(filename);
		return -1;
	}
	rc = fslurp(fd, b);
	close(fd);
	return rc;
}

int fslurp(int fd, struct buf *b)
{
	struct stat st;
	char *tmp;
	int rc;

	if(fstat(fd, &st) < 0) {
		perror("fstat");
		return -1;
	}

	tmp = malloc(st.st_size);
	if(!tmp) {
		perror("malloc");
		return -1;
	}

	rc = read(fd, tmp, st.st_size);
	if(rc < 0) {
		perror("read");
		goto err_out;
	}
	if(rc != st.st_size) {
		fprintf(stderr, "read error: bytes read (%i) != bytes expected "
			"(%i)\n", rc, (int)st.st_size);
		goto err_out;
	}

	b->s = tmp;
	b->len = st.st_size;
	return 0;

err_out:
	free(tmp);
	return -1;
}
