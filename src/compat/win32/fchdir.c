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
#include <string.h>
#include <errno.h>
#include "dirpath.h"
#include "tup/compat.h"

int fchdir(int fd)
{
	const char *path;

	path = win32_get_dirpath(fd);
	if(path) {
		/* If we are changing to a dir like "C:", then we have to make sure we also
		 * clear the path afterwards. Otherwise just cd'ing to C: will put is back
		 * at whatever last directory we were at in C:, not necessarily at C:\
		 */
		if(is_full_path(path) && strlen(path) == 2) {
			if(chdir(path) < 0)
				return -1;
			return chdir("\\");
		}
		return chdir(path);
	}
	errno = EBADF;
	return -1;
}
