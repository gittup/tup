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
	static char cname[PATH_MAX];
	int type;

	if(argc < 4) {
		fprintf(stderr, "Usage: %s input/output read_file write_file\n", argv[0]);
		return 1;
	}
	if(find_tup_dir() < 0)
		return 1;

	if(strcmp(argv[1], "input") == 0) {
		type = 0;
	} else if(strcmp(argv[1], "output") == 0) {
		type = 1;
	} else {
		fprintf(stderr, "Type must be input or output.\n");
		return 1;
	}

	if(type == 0) {
		if(canonicalize(argv[2], cname, sizeof(cname)) < 0) {
			fprintf(stderr, "Unable to canonicalize '%s'\n", argv[2]);
			return 1;
		}
		if(create_name_file(cname) < 0)
			return 1;
		tupid_from_filename(a, cname);
	} else {
		if(create_command_file(argv[2]) < 0)
			return 1;
		tupid_from_filename(a, argv[2]);
	}

	if(type == 1) {
		if(canonicalize(argv[3], cname, sizeof(cname)) < 0) {
			fprintf(stderr, "Unable to canonicalize '%s'\n", argv[3]);
			return 1;
		}
		if(create_name_file(cname) < 0)
			return 1;
		tupid_from_filename(b, cname);
	} else {
		if(create_command_file(argv[3]) < 0)
			return 1;
		tupid_from_filename(b, argv[3]);
	}

	if(type == 0) {
		if(create_command_link(a, b) < 0)
			return 1;
	} else {
		if(create_primary_link(a, b) < 0)
			return 1;
	}

	return 0;
}
