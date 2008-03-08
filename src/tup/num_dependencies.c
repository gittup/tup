#include "fileio.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

int num_dependencies(const tupid_t tupid)
{
	struct stat buf;
	char tupfilename[] = ".tup/object/" SHA1_XD "/.name";

	tupid_to_xd(tupfilename + 12, tupid);
	if(stat(tupfilename, &buf) < 0) {
		perror(tupfilename);
		return -1;
	}
	return buf.st_nlink - 1;
}
