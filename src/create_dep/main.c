/* Utility to create an edge in the graph.
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "tup/tupid.h"
#include "tup/fileio.h"

int main(int argc, char **argv)
{
	tupid_t a;
	tupid_t b;

	if(argc < 3) {
		fprintf(stderr, "Usage: %s read_file write_file\n", argv[0]);
		return 1;
	}

	tupid_from_filename(a, argv[1]);
	tupid_from_filename(b, argv[2]);
	if(write_sha1dep(b, a) < 0)
		return 1;
	return 0;
}
