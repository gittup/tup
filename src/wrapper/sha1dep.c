#include "sha1dep.h"
#include "debug.h"
#include "fileio.h"
#include "mkdirhier.h"

#include <stdio.h>
#include <string.h>

int write_sha1dep(const tupid_t file, const tupid_t depends_on)
{
	char depfilename[] = ".tup/object/" SHA1_X "/" SHA1_X;

	memcpy(depfilename + 12, depends_on, sizeof(tupid_t));
	memcpy(depfilename + 53, file, sizeof(tupid_t));

	DEBUGP("create dependency: %s\n", depfilename);

	if(mkdirhier(depfilename) < 0) {
		return -1;
	}

	return create_if_not_exist(depfilename);
}
