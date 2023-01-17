/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2010-2023  Mike Shal <marfey@gmail.com>
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

#include "dir_mutex.h"
#include "tup/compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

pthread_mutex_t dir_mutex = PTHREAD_MUTEX_INITIALIZER;
int dir_mutex_enabled = 1;

void compat_lock_enable(void)
{
	dir_mutex_enabled = 1;
}

void compat_lock_disable(void)
{
	dir_mutex_enabled = 0;
}

int dir_mutex_lock(int dfd)
{
	if(dir_mutex_enabled)
		pthread_mutex_lock(&dir_mutex);

	if(fchdir(dfd) < 0) {
		dir_mutex_unlock();
		if(errno == EBADF) {
			/* get_outside_tup_mtime expects ENOENT or ENOTDIR, not
			 * EBADF, which is what an fchdir on a non-directory
			 * file descriptor gives.
			 */
			errno = ENOENT;
		}
		return -errno;
	}
	return 0;
}

void dir_mutex_unlock(void)
{
	if(dir_mutex_enabled)
		pthread_mutex_unlock(&dir_mutex);
}
