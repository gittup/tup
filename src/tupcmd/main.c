#include <stdio.h>
#include "tup/compat.h"
#include "tup/fileio.h"
#include "tup/config.h"

int main(int argc, char **argv)
{
	tupid_t ct;
	tupid_t dt;

	if(argc != 2) {
		fprintf(stderr, "Usage: %s cmd\n", argv[0]);
		fprintf(stderr, " cmd is the command to create, with optional arguments\n");
		return 1;
	}
	if(find_tup_dir() < 0) {
		return 1;
	}

	if(create_command_file(argv[1]) < 0)
		return 1;

	tupid_from_filename(ct, argv[1]);
	tupid_from_filename(dt, get_sub_dir());
	if(create_command_link(dt, ct) < 0)
		return 1;
	return 0;
}
