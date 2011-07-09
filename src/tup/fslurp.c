#include "fslurp.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

static int do_slurp(int fd, struct buf *b, int extra);

int fslurp(int fd, struct buf *b)
{
	return do_slurp(fd, b, 0);
}

int fslurp_null(int fd, struct buf *b)
{
	int rc;

	rc = do_slurp(fd, b, 1);
	if(rc == 0)
		b->s[b->len] = 0;
	return rc;
}

static int do_slurp(int fd, struct buf *b, int extra)
{
	struct stat st;
	char *tmp;
	int rc;

	if(fstat(fd, &st) < 0) {
		perror("fstat");
		return -1;
	}

	tmp = malloc(st.st_size + extra);
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
		fprintf(stderr, "tup error: read %i bytes, but expected %lli bytes\n", rc, st.st_size);
		errno = EIO;
		goto err_out;
	}

	b->s = tmp;
	b->len = st.st_size;
	return 0;

err_out:
	free(tmp);
	return -1;
}
