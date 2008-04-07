#include <stdio.h>
#include <libgen.h> /* TODO: dirname */
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
		char *slash;
		char *dir;
		if(canonicalize(argv[x], cname, sizeof(cname)) < 0) {
			fprintf(stderr, "Unable to canonicalize '%s'\n",
				argv[x]);
			return 1;
		}
		if(create_name_file(cname) < 0)
			return 1;
		if(create_tup_file(argv[1], cname) < 0)
			return 1;
		slash = strrchr(argv[1], '/');
		if(slash)
			*slash = 0;
		dir = dirname(argv[1]);
		if(create_dir_file(dir) < 0)
			return 1;
	}
	return 0;
}
