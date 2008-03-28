#include <stdio.h>
#include "tup/compat.h"
#include "tup/fileio.h"
#include "tup/config.h"

int main(int argc, char **argv)
{
	int x;
	static char cname[PATH_MAX];

	if(argc < 3) {
		fprintf(stderr, "Usage: %s type filename\n", argv[0]);
		fprintf(stderr, " where 'type' is one of 'create', 'delete', or 'modify'\n");
		return 1;
	}
	if(find_tup_dir() < 0)
		return 1;

	for(x=2; x<argc; x++) {
		if(canonicalize(argv[x], cname, sizeof(cname)) < 0) {
			fprintf(stderr, "Unable to canonicalize '%s'\n",
				argv[x]);
			return 1;
		}
		if(create_name_file(cname) < 0)
			return 1;
		if(create_tup_file(argv[1], cname) < 0)
			return 1;
	}
	return 0;
}
