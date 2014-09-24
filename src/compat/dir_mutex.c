/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2010-2016  Mike Shal <marfey@gmail.com>
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

pthread_mutex_t dir_mutex;
int dir_mutex_enabled = 1;

int compat_init(void)
{
	if(pthread_mutex_init(&dir_mutex, NULL) < 0)
		return -1;
	return 0;
}

void compat_lock_enable(void)
{
	dir_mutex_enabled = 1;
}

void compat_lock_disable(void)
{
	dir_mutex_enabled = 0;
}

void dir_mutex_lock(int dfd)
{
	if(dir_mutex_enabled)
		pthread_mutex_lock(&dir_mutex);

	// AT_FDCWD
	if (dfd == -100)
		return;

	if(fchdir(dfd) < 0) {
		perror("fchdir");
		fprintf(stderr, "tup error: Failed to fchdir in a compat wrapper function at %p.\n", __builtin_return_address(0));
		exit(1);
	}
}

void dir_mutex_unlock(void)
{
	if(dir_mutex_enabled)
		pthread_mutex_unlock(&dir_mutex);
}
