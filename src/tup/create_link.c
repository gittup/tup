#include "fileio.h"
#include "debug.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

int create_link(const tupid_t a, const tupid_t b)
{
	char depfilename[] = ".tup/object/" SHA1_XD "/" SHA1_X;
	char namefile[] = ".tup/object/" SHA1_XD "/.name";

	tupid_to_xd(depfilename + 12, a);
	memcpy(depfilename + 14 + sizeof(tupid_t), b, sizeof(tupid_t));

	tupid_to_xd(namefile + 12, b);
	DEBUGP("create primary link: %.*s -> %.*s\n", 8, a, 8, b);
	unlink(depfilename);
	if(link(namefile, depfilename) < 0) {
		fprintf(stderr, "link %s -> %s: %s\n",
			depfilename, namefile, strerror(errno));
		return -1;
	}
	return 0;
}

int create_command_link(const tupid_t a, const tupid_t b)
{
	char depfilename[] = ".tup/object/" SHA1_XD "/" SHA1_X;
	char namefile[] = ".tup/object/" SHA1_XD "/.cmd";

	tupid_to_xd(depfilename + 12, a);
	memcpy(depfilename + 14 + sizeof(tupid_t), b, sizeof(tupid_t));

	tupid_to_xd(namefile + 12, b);
	DEBUGP("create command link: %.*s -> %.*s\n", 8, a, 8, b);
	unlink(depfilename);
	if(link(namefile, depfilename) < 0) {
		fprintf(stderr, "link %s -> %s: %s\n",
			depfilename, namefile, strerror(errno));
		return -1;
	}
	return 0;
}
