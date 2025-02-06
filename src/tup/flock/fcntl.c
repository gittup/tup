/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2024  Mike Shal <marfey@gmail.com>
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

#ifdef USE_DOTLOCK
#include "tup/config.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <dlfcn.h>

#define dotlock_path_len 256


static int get_dotlockname(int fd, char *name, size_t len)
{
	struct stat file_stat;
	int ret;
	ret = fstat (fd, &file_stat);
	if (ret < 0) return -1;

	int inode = file_stat.st_ino;  // inode now contains inode number of the file with descriptor fd

	// todo: .tup from config.h?
	//size_t sz = snprintf(name, len, "/run/lock/tup-%d.lock", inode);
	size_t sz = snprintf(name, len, "/tmp/tup-%d.lock", inode);
	if (sz >= len) return -1;

	return 0;
}



static int make_dotlock(int fd, int wait_type)
{
	int prev_errno = errno;
	int ret = -1;
	do {
		char dotlockname[dotlock_path_len];
		get_dotlockname(fd, dotlockname, dotlock_path_len);
		//if ((fdl = open(dotlockname, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0)
		//int fdl = open(dotlockname, O_WRONLY | O_CREAT | O_EXCL, 0666);
		int fdl = mkdir(dotlockname, 0666);

		if (fdl < 0)
		{
			if (errno == EEXIST) 
			{
				ret = 1;
			}
			else
			{
				ret = -1;
				break;
			}
		}
		else 
		{
			ret = 0;
			break;
		}
		//close(fdl);
		if (wait_type == 1) sleep(1);
		else if (wait_type == 2) usleep(10000);
	} while (wait_type > 0); 
	errno = prev_errno;
	return ret;
}

static int remove_dotlock(int fd)
{
	char dotlockname[dotlock_path_len];
	get_dotlockname(fd, dotlockname, dotlock_path_len);
	//if (unlink(dotlockname) < 0)
	if (rmdir(dotlockname) < 0)
	{
		return -1;
	}
	return 0;
}



#endif

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
#ifdef USE_DOTLOCK
	//char dotlockname[dotlock_path_len];
	//get_dotlockname(fd, dotlockname, dotlock_path_len);
	//while (mkdir(dotlockname, 0666) < 0) {
	if (make_dotlock(fd, 1) < 0) return -1;
#else
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
#endif
	return 0;
}

/* Returns: -1 error, 0 got lock, 1 would block */
int tup_try_flock(tup_lock_t fd)
{
#ifdef USE_DOTLOCK
	//char dotlockname[dotlock_path_len];
	//get_dotlockname(fd, dotlockname, dotlock_path_len);
	//if (mkdir(dotlockname, 0666) < 0) {
	int mdl = make_dotlock(fd, 0);
	if (mdl != 0) return mdl;
#else
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
#endif
	return 0;
}

int tup_unflock(tup_lock_t fd)
{
#ifdef USE_DOTLOCK
	//char dotlockname[dotlock_path_len];
	//get_dotlockname(fd, dotlockname, dotlock_path_len);
	//if (rmdir(dotlockname) < 0) {
	if (remove_dotlock(fd) < 0) {
		perror("rm dotlock");
		return -1;
	}
#else
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
#endif
	return 0;
}

int tup_wait_flock(tup_lock_t fd)
{
#ifdef USE_DOTLOCK
	//char dotlockname[dotlock_path_len];
	//get_dotlockname(fd, dotlockname, dotlock_path_len);
	//while (mkdir(dotlockname, 0666) < 0) {
	if (make_dotlock(fd, 2) < 0) return -1;
#else
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
#endif
	return 0;
}

