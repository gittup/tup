/* Utility to create an edge in the graph. */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include "tup/tupid.h"
#include "tup/fileio.h"
#include "tup/compat.h"

int main(int argc, char **argv)
{
	tupid_t a;
	tupid_t b;
	static char cname[PATH_MAX];

	if(argc < 3) {
		fprintf(stderr, "Usage: %s read_file write_file\n", argv[0]);
		return 1;
	}

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

	return 0;
}
