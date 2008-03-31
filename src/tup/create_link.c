#include "fileio.h"
#include "debug.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

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
		fprintf(stderr, "link %s -> %s: %s\n",
			namefile, depfilename, strerror(errno));
		return -1;
	}
	return 0;
}

int create_secondary_link(const tupid_t a, const tupid_t b)
{
	struct stat st;
	char depfilename[] = ".tup/object/" SHA1_XD "/" SHA1_X;
	char namefile[] = ".tup/object/" SHA1_XD "/.secondary";

	tupid_to_xd(depfilename + 12, a);
	memcpy(depfilename + 14 + sizeof(tupid_t), b, sizeof(tupid_t));

	tupid_to_xd(namefile + 12, b);

	/* Unlink the dependency if it's already a secondary dependency, since
	 * the .secondary file will have been re-created. Note we know if it's
	 * a secondary dep because the size will be zero.
	 */
	if(stat(depfilename, &st) == 0 && st.st_size == 0) {
		unlink(depfilename);
	}
	DEBUGP("create secondary link: %.*s -> %.*s\n", 8, a, 8, b);
	if(link(namefile, depfilename) < 0 && errno != EEXIST) {
		perror(depfilename);
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
			namefile, depfilename, strerror(errno));
		return -1;
	}
	return 0;
}

#if 0
int create_link_link(const tupid_t a, const tupid_t b)
{
	char depfilename[] = ".tup/object/" SHA1_XD "/" SHA1_X;
	char namefile[] = ".tup/object/" SHA1_XD "/.link";

	tupid_to_xd(depfilename + 12, a);
	memcpy(depfilename + 14 + sizeof(tupid_t), b, sizeof(tupid_t));

	tupid_to_xd(namefile + 12, b);
	DEBUGP("create link link: %.*s -> %.*s\n", 8, a, 8, b);
	unlink(depfilename);
	if(link(namefile, depfilename) < 0) {
		fprintf(stderr, "link %s -> %s: %s\n",
			namefile, depfilename, strerror(errno));
		return -1;
	}
	return 0;
}
#endif
