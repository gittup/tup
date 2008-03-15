#include "fileio.h"
#include "debug.h"

#include <stdio.h>
#include <string.h>

int write_sha1dep(const tupid_t file, const tupid_t depends_on)
{
	char depfilename[] = ".tup/object/" SHA1_XD "/" SHA1_X;
	char namefile[] = ".tup/object/" SHA1_XD "/.name";

	tupid_to_xd(depfilename + 12, depends_on);
	memcpy(depfilename + 14 + sizeof(tupid_t), file, sizeof(tupid_t));

	tupid_to_xd(namefile + 12, file);

	DEBUGP("create dependency: %s\n", depfilename);

	unlink(depfilename);
	if(link(namefile, depfilename) < 0) {
		perror(depfilename);
		return -1;
	}
	return 0;
}
