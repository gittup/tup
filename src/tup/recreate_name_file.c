#include "fileio.h"
#include "compat.h"
#include "debug.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int recreate_name_file(const tupid_t tupid)
{
	int fd;
	int rc = -1;
	int nb;
	char tupfilename[] = ".tup/object/" SHA1_XD "/.name";
	static char buf[PATH_MAX];

	tupid_to_xd(tupfilename + 12, tupid);

	DEBUGP("recreate name file '%s'\n", tupfilename);

	fd = open(tupfilename, O_RDONLY);
	if(fd < 0) {
		perror(tupfilename);
		return -1;
	}
	nb = read(fd, buf, sizeof(buf));
	close(fd);

	unlink(tupfilename);
	fd = open(tupfilename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if(fd < 0) {
		perror(tupfilename);
		return -1;
	}
	rc = write(fd, buf, nb);
	if(rc < 0) {
		perror("write");
		goto err_out;
	}
	if(rc != nb) {
		fprintf(stderr, "Error: didn't write all %i bytes to %s\n",
			nb, tupfilename);
		goto err_out;
	}
	rc = 0;
err_out:
        close(fd);
        return rc;
}
