#include "fileio.h"
#include "debug.h"

#include <stdio.h>
#include <string.h>

int write_sha1dep(const tupid_t file, const tupid_t depends_on)
{
	char depfilename[] = ".tup/object/" SHA1_XD "/" SHA1_X;
	char linkfilename[] = ".tup/object/" SHA1_XD "/.name";

	tupid_to_xd(depfilename + 12, depends_on);
	memcpy(depfilename + 14 + sizeof(tupid_t), file, sizeof(tupid_t));

	tupid_to_xd(linkfilename + 12, file);

	DEBUGP("create dependency: %s\n", depfilename);

	if(mkdirhier(depfilename) < 0) {
		return -1;
	}

	return link_if_not_exist(linkfilename, depfilename);
}
