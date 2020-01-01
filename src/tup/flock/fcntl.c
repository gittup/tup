/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2020  Mike Shal <marfey@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define _ATFILE_SOURCE
#include "tup/flock.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int tup_lock_open(int basefd, const char *lockname, tup_lock_t *lock)
{
	int fd;

	fd = openat(basefd, lockname, O_RDWR | O_CREAT, 0666);
	if(fd < 0) {
		perror(lockname);
		fprintf(stderr, "tup error: Unable to open lockfile.\n");
		return -1;
	}
	*lock = fd;
	return 0;
}

void tup_lock_close(tup_lock_t lock)
{
	if(close(lock) < 0) {
		perror("close(lock)");
	}
}

int tup_flock(tup_lock_t fd)
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

/* Returns: -1 error, 0 got lock, 1 would block */
int tup_try_flock(tup_lock_t fd)
{
	struct flock fl = {
		.l_type = F_WRLCK,
		.l_whence = SEEK_SET,
		.l_start = 0,
		.l_len = 0,
	};

	if(fcntl(fd, F_SETLK, &fl) < 0) {
		if (errno == EAGAIN)
			return 1;
		perror("fcntl F_WRLCK");
		return -1;
	}
	return 0;
}

int tup_unflock(tup_lock_t fd)
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

int tup_wait_flock(tup_lock_t fd)
{
	struct flock fl;

	while(1) {
		fl.l_type = F_WRLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = 0;
		fl.l_len = 0;

		if(fcntl(fd, F_GETLK, &fl) < 0) {
			perror("fcntl F_GETLK");
			return -1;
		}

		if(fl.l_type == F_WRLCK)
			break;
		usleep(10000);
	}
	return 0;
}
