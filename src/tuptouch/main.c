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
		const char *type;
		if(canonicalize(argv[x], cname, sizeof(cname)) < 0) {
			fprintf(stderr, "Unable to canonicalize '%s'\n",
				argv[x]);
			return 1;
		}
		if(create_name_file(cname) < 0)
			return 1;

		/* TODO: alias create -> modify in the script? */
		if(strcmp(argv[1], "create") == 0)
			type = "modify";
		else
			type = argv[1];
		if(create_tup_file(type, cname) < 0)
			return 1;
		if(strcmp(argv[1], "delete") == 0) {
			char *slash;
			char *dir;
			slash = strrchr(cname, '/');
			if(slash) {
				*slash = 0;
			}
			dir = dirname(cname);
			if(create_dir_file(dir) < 0)
				return 1;
		}
	}
	return 0;
}
