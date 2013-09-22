/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011  Mike Shal <marfey@gmail.com>
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

#include "tup/flock.h"
#include <windows.h>
#include <errno.h>

int tup_flock(int fd)
{
	HANDLE h;
	long int tmp;
	OVERLAPPED wtf;

	tmp = _get_osfhandle(fd);
	h = (HANDLE)tmp;

	memset(&wtf, 0, sizeof(wtf));
	if(LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &wtf) == 0) {
		errno = EIO;
		return -1;
	}
	return 0;
}

/* Returns: -1 error, 0 got lock, 1 would block */
int tup_try_flock(int fd)
{
	HANDLE h;
	long int tmp;
	OVERLAPPED wtf;

	tmp = _get_osfhandle(fd);
	h = (HANDLE)tmp;

	memset(&wtf, 0, sizeof(wtf));
	if(LockFileEx(h, LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &wtf) == 0) {
		DWORD last_error;

		last_error = GetLastError();
		if (last_error == ERROR_IO_PENDING) {
			errno = EAGAIN;
			return 1;
		}
		errno = EIO;
		return -1;
	}
	return 0;
}

int tup_unflock(int fd)
{
	HANDLE h;
	long int tmp;

	tmp = _get_osfhandle(fd);
	h = (HANDLE)tmp;
	if(UnlockFile(h, 0, 0, 1, 0) == 0) {
		errno = EIO;
		return -1;
	}
	return 0;
}

int tup_wait_flock(int fd)
{
	if(fd) {}
	/* Unsupported - only used by inotify file monitor */
	return -1;
}
