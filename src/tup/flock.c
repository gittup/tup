#include "flock.h"
#include <stdio.h>
#include <fcntl.h>

int tup_flock(int fd)
{
	struct flock fl = {
		.l_type = F_WRLCK,
		.l_whence = SEEK_SET,
		.l_start = 0,
		.l_len = 0,
	};

	if(fcntl(fd, F_SETLKW, &fl) < 0) {
		perror("fcntl F_WRLCK");
		return -1;
	}
	return 0;
}

int tup_unflock(int fd)
{
	struct flock fl = {
		.l_type = F_UNLCK,
		.l_whence = SEEK_SET,
		.l_start = 0,
		.l_len = 0,
	};

	if(fcntl(fd, F_SETLKW, &fl) < 0) {
		perror("fcntl F_UNLCK");
		return -1;
	}
	return 0;
}
