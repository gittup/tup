/* Utility to create an edge in the graph. */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include "tup/tupid.h"
#include "tup/fileio.h"
#include "tup/compat.h"
#include "tup/config.h"

int main(int argc, char **argv)
{
	tupid_t a;
	tupid_t b;
	tupid_t lt;
	tupid_t dt;
	static char cname[PATH_MAX];
	char linkname[] = SHA1_X "->" SHA1_X;

	if(argc < 3) {
		fprintf(stderr, "Usage: %s read_file write_file\n", argv[0]);
		return 1;
	}
	if(find_tup_dir() < 0)
		return 1;

	if(canonicalize(argv[1], cname, sizeof(cname)) < 0) {
		fprintf(stderr, "Unable to canonicalize '%s'\n", argv[1]);
		return 1;
	}
	if(create_name_file(cname) < 0)
		return 1;
	tupid_from_filename(a, cname);

	if(canonicalize(argv[2], cname, sizeof(cname)) < 0) {
		fprintf(stderr, "Unable to canonicalize '%s'\n", argv[2]);
		return 1;
	}
	if(create_name_file(cname) < 0)
		return 1;
	tupid_from_filename(b, cname);

	if(create_primary_link(a, b) < 0)
		return 1;

	memcpy(linkname, a, sizeof(tupid_t));
	memcpy(linkname + sizeof(tupid_t) + 2, b, sizeof(tupid_t));
	tupid_from_filename(lt, linkname);
	tupid_from_filename(dt, get_sub_dir());
	if(create_name_file(linkname) < 0)
		return 1;
	if(create_primary_link(dt, lt) < 0)
		return 1;

	return 0;
}
