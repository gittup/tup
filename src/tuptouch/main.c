#include <stdio.h>
#include "tup/compat.h"
#include "tup/fileio.h"
#include "tup/config.h"

int main(int argc, char **argv)
{
	static char out[PATH_MAX];
	char *path;
	int x;

	if(argc < 3) {
		fprintf(stderr, "Usage: %s type filename\n", argv[0]);
		fprintf(stderr, " where 'type' is one of 'create', 'delete', or 'modify'\n");
		return 1;
	}
	if(find_tup_dir() < 0) {
		return 1;
	}

	for(x=2; x<argc; x++) {
		path = argv[x];
		if(path[0] == '/') {
			if(canonicalize(path, "", out, sizeof(out)) < 0)
				return 1;
		} else {
			if(canonicalize("", path, out, sizeof(out)) < 0)
				return 1;
		}
		if(create_name_file(out) < 0)
			return 1;
		if(create_tup_file(argv[1], out, "") < 0)
			return 1;
	}
	return 0;
}
