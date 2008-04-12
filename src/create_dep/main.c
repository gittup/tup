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
	tupid_t cmdid;
	tupid_t t;
	static char cname[PATH_MAX];
	int type;
	int x;

	if(argc < 4) {
		fprintf(stderr, "Usage: %s cmd -iread_file -owrite_file\n", argv[0]);
		return 1;
	}
	if(find_tup_dir() < 0)
		return 1;

	tupid_from_filename(cmdid, argv[1]);
	if(create_command_file(argv[1]) < 0)
		return 1;
	tupid_from_filename(t, get_sub_dir());
	if(create_command_link(t, cmdid) < 0)
		return 1;

	for(x=2; x<argc; x++) {
		char *name = argv[x];
		if(name[0] == '-') {
			if(name[1] == 'i') {
				type = 0;
			} else if(name[1] == 'o') {
				type = 1;
			} else {
				fprintf(stderr, "Invalid argument: '%s'\n",
					name);
				return 1;
			}
		} else {
			fprintf(stderr, "Invalid argument: '%s'\n", name);
			return 1;
		}
		if(canonicalize(name+2, cname, sizeof(cname)) < 0) {
			fprintf(stderr, "Unable to canonicalize '%s'\n", argv[2]);
			return 1;
		}
		if(create_name_file(cname) < 0)
			return 1;
		tupid_from_filename(t, cname);

		if(type == 0) {
			if(create_command_link(t, cmdid) < 0)
				return 1;
		} else {
			if(create_primary_link(cmdid, t) < 0)
				return 1;
		}
	}

	return 0;
}
