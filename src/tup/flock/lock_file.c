/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2017  Mike Shal <marfey@gmail.com>
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
#include <stdio.h>
#include <errno.h>

int tup_lock_open(const char *lockname, tup_lock_t *lock)
{
	HANDLE h;
	wchar_t wlockname[PATH_MAX];

	MultiByteToWideChar(CP_UTF8, 0, lockname, -1, wlockname, PATH_MAX);

	h = CreateFile(wlockname,
		       GENERIC_READ | GENERIC_WRITE,
		       FILE_SHARE_READ | FILE_SHARE_WRITE,
		       NULL,
		       OPEN_ALWAYS,
		       0,
		       NULL);
	if(h == INVALID_HANDLE_VALUE) {
		perror(lockname);
		fprintf(stderr, "tup error: Unable to open lockfile.\n");
		return -1;
	}
	*lock = h;
	return 0;
}

void tup_lock_close(tup_lock_t lock)
{
	CloseHandle(lock);
}

int tup_flock(tup_lock_t fd)
{
	OVERLAPPED wtf = {0};

	if(LockFileEx(fd, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &wtf) == 0) {
		errno = EIO;
		return -1;
	}
	return 0;
}

/* Returns: -1 error, 0 got lock, 1 would block */
int tup_try_flock(tup_lock_t fd)
{
	OVERLAPPED wtf = {0};

	if(LockFileEx(fd, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &wtf) == 0) {
		DWORD last_error;

		last_error = GetLastError();
		if(last_error == ERROR_LOCK_VIOLATION) {
			errno = EAGAIN;
			return 1;
		}
		errno = EIO;
		return -1;
	}
	return 0;
}

int tup_unflock(tup_lock_t fd)
{
	if(UnlockFile(fd, 0, 0, 1, 0) == 0) {
		errno = EIO;
		return -1;
	}
	return 0;
}

int tup_wait_flock(tup_lock_t fd)
{
	if(fd) {}
	/* Unsupported - only used by inotify file monitor */
	return -1;
}
