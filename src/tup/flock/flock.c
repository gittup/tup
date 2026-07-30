/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2026  Mike Shal <marfey@gmail.com>
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

/* BSD flock(2) backend. Used on macOS because kqueue EVFILT_VNODE with
 * NOTE_FUNLOCK can detect flock releases (but not fcntl lock releases),
 * enabling event-driven lock coordination in the monitor.
 */

#define _ATFILE_SOURCE
#include "tup/flock.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/file.h>

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
	if(flock(fd, LOCK_EX) < 0) {
		perror("flock LOCK_EX");
		return -1;
	}
	return 0;
}

/* Returns: -1 error, 0 got lock, 1 would block */
int tup_try_flock(tup_lock_t fd)
{
	if(flock(fd, LOCK_EX | LOCK_NB) < 0) {
		if(errno == EWOULDBLOCK)
			return 1;
		perror("flock LOCK_EX|LOCK_NB");
		return -1;
	}
	return 0;
}

int tup_unflock(tup_lock_t fd)
{
	if(flock(fd, LOCK_UN) < 0) {
		perror("flock LOCK_UN");
		return -1;
	}
	return 0;
}

int tup_wait_flock(tup_lock_t fd)
{
	/* Not used on macOS - the fsevents monitor uses kqueue
	 * NOTE_FUNLOCK/NOTE_ATTRIB for event-driven lock notification
	 * instead of polling. It is not possible to achieve this API on
	 * macOS without polling.
	 */
	(void)fd;
	fprintf(stderr, "tup internal error: tup_wait_flock called on macOS\n");
	return -1;
}
