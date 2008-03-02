#include "fileio.h"
#include "debug.h"

#include <stdio.h>
#include <string.h>

int write_sha1dep(const tupid_t file, const tupid_t depends_on)
{
	char depfilename[] = ".tup/object/" SHA1_X "/" SHA1_X;
	char linkfilename[] = ".tup/object/" SHA1_X "/.name";

	memcpy(depfilename + 12, depends_on, sizeof(tupid_t));
	memcpy(depfilename + 13 + sizeof(tupid_t), file, sizeof(tupid_t));

	memcpy(linkfilename + 12, file, sizeof(tupid_t));

	DEBUGP("create dependency: %s\n", depfilename);

	if(mkdirhier(depfilename) < 0) {
		return -1;
	}

	return link_if_not_exist(linkfilename, depfilename);
}
