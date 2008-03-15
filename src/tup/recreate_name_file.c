#include "fileio.h"
#include "compat.h"
#include "debug.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int recreate_name_file(const tupid_t tupid)
{
	int fd;
	char depfilename[] = ".tup/object/" SHA1_XD "/.secondary";

	tupid_to_xd(depfilename + 12, tupid);

	DEBUGP("recreate name file '%s'\n", depfilename);

	unlink(depfilename);
	fd = open(depfilename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if(fd < 0) {
		perror(depfilename);
		return -1;
	}
	close(fd);

	return 0;
}
