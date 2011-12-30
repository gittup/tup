/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2010-2011  Mike Shal <marfey@gmail.com>
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
#include <windows.h>
#include "dirpath.h"
#include "open_notify.h"

int __wrap_open(const char *pathname, int flags, ...);
int __real_open(const char *pathname, int flags, ...);

int __wrap_open(const char *pathname, int flags, ...)
{
	mode_t mode = 0;
	enum access_type at = ACCESS_READ;

	if(flags & O_WRONLY || flags & O_RDWR)
		at = ACCESS_WRITE;
	if(open_notify(at, pathname) < 0)
		return -1;

	if(flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	} else {
		DWORD attributes;

		attributes = GetFileAttributesA(pathname);

		/* If there was an error getting the file attributes, or if we
		 * are trying to open a normal file, we want to fall through to
		 * the __real_open case. Only things that we know are
		 * directories go through the special dirpath logic.
		 */
		if(attributes != INVALID_FILE_ATTRIBUTES) {
			if(attributes & FILE_ATTRIBUTE_DIRECTORY) {
				return win32_add_dirpath(pathname);
			}
		}
	}
	return __real_open(pathname, flags | O_BINARY, mode);
}
