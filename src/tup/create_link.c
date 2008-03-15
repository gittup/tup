#include "fileio.h"
#include "debug.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

int create_primary_link(const tupid_t a, const tupid_t b)
{
	char depfilename[] = ".tup/object/" SHA1_XD "/" SHA1_X;
	char namefile[] = ".tup/object/" SHA1_XD "/.name";

	tupid_to_xd(depfilename + 12, a);
	memcpy(depfilename + 14 + sizeof(tupid_t), b, sizeof(tupid_t));

	tupid_to_xd(namefile + 12, b);
	DEBUGP("create primary link: %.*s -> %.*s\n", 8, a, 8, b);
	unlink(depfilename);
	if(link(namefile, depfilename) < 0) {
		perror(depfilename);
		return -1;
	}
	return 0;
}

int create_secondary_link(const tupid_t a, const tupid_t b)
{
	char depfilename[] = ".tup/object/" SHA1_XD "/" SHA1_X;
	char namefile[] = ".tup/object/" SHA1_XD "/.secondary";

	tupid_to_xd(depfilename + 12, a);
	memcpy(depfilename + 14 + sizeof(tupid_t), b, sizeof(tupid_t));

	tupid_to_xd(namefile + 12, b);
	DEBUGP("create secondary link: %.*s -> %.*s\n", 8, a, 8, b);
	if(link(namefile, depfilename) < 0 && errno != EEXIST) {
		perror(depfilename);
		return -1;
	}
	return 0;
}
