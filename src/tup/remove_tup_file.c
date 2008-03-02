#include "fileio.h"
#include <string.h>

int remove_tup_file(const char *tup, const tupid_t tupid)
{
	char filename[] = ".tup/XXXXXX/" SHA1_X;

	memcpy(filename + 5, tup, 6);
	memcpy(filename + 12, tupid, sizeof(tupid_t));

	return remove_if_exists(filename);
}
