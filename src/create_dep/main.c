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
#if 0
	int lock_fd;
#endif

	tupid_t a;
	tupid_t b;

	if(argc < 3) {
		fprintf(stderr, "Usage: %s read_file write_file\n", argv[0]);
		return 1;
	}

#if 0
	lock_fd = open(TUP_LOCK, O_RDONLY);
	if(lock_fd < 0) {
		perror(TUP_LOCK);
		return 1;
	}
	if(flock(lock_fd, LOCK_EX) < 0) {
		perror("flock");
		return 1;
	}
#endif

	if(create_name_file(argv[1]) < 0)
		return 1;
	if(create_name_file(argv[2]) < 0)
		return 1;

	tupid_from_filename(a, argv[1]);
	tupid_from_filename(b, argv[2]);
	if(create_primary_link(a, b) < 0)
		return 1;

#if 0
	close(lock_fd);
#endif
	return 0;
}
