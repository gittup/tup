#include <stdio.h>
#include "tup/compat.h"
#include "tup/fileio.h"
#include "tup/config.h"

int main(int argc, char **argv)
{
	static char filename[PATH_MAX];
	char *path;

	if(argc < 3) {
		fprintf(stderr, "Usage: %s type filename\n", argv[0]);
		fprintf(stderr, " where 'type' is one of 'create', 'delete', or 'modify'\n");
		return 1;
	}
	if(find_tup_dir() < 0) {
		return 1;
	}
	path = argv[2];
	if(path[0] == '/') {
		if(canonicalize(path, "", filename, sizeof(filename)) < 0)
			return 1;
	} else {
		if(canonicalize("", path, filename, sizeof(filename)) < 0)
			return 1;
	}
	if(create_name_file(filename) < 0)
		return 1;
	if(create_tup_file(argv[1], filename, "") < 0)
		return 1;
	return 0;
}
