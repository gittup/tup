#include "fileio.h"
#include "debug.h"
#include <stdio.h>
#include <string.h>

int delete_tup_file(const char *tup, const tupid_t tupid)
{
	char filename[] = ".tup/XXXXXX/" SHA1_X;

	DEBUGP("delete tup file %s/%.*s\n", tup, 8, tupid);
	memcpy(filename + 5, tup, 6);
	memcpy(filename + 12, tupid, sizeof(tupid_t));

	return delete_if_exists(filename);
}
