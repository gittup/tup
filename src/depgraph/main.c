#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include "tup/graph.h"
#include "tup/compat.h"

int main(void)
{
	struct graph g;
	int lock_fd;

	lock_fd = open(TUP_LOCK, O_RDONLY);
	if(lock_fd < 0) {
		perror(TUP_LOCK);
		return 1;
	}
	if(flock(lock_fd, LOCK_SH) < 0) {
		perror("flock");
		return 1;
	}

	if(create_graph(&g, TUPDIR_HASH) < 0)
		return -1;

	flock(lock_fd, LOCK_UN);
	close(lock_fd);
	return 0;
}
