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

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "tup/compat.h"
#include "dir_mutex.h"

#ifdef _WIN32
#define mkdir(a,b) mkdir(a)
#endif

int mkdirat(int dirfd, const char *pathname, mode_t mode)
{
	int rc;
#ifdef _WINDOWS
	char cwd[PATH_MAX];
	getcwd(cwd, PATH_MAX);
#endif
	if(mode) {/* for win32 */}

	dir_mutex_lock(dirfd);
	rc = mkdir(pathname, mode);
#ifdef _WINDOWS
	chdir(cwd);
#endif
	dir_mutex_unlock();
	return rc;
}
