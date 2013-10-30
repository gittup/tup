/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2013  Mike Shal <marfey@gmail.com>
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
#include <stdarg.h>
#include <dlfcn.h>
#include "compat/open_notify.h"

static int (*real_open)(const char *pathname, int flags, ...);

void __attribute__((constructor)) init_open(void);
void __attribute__((constructor)) init_open(void)
{
	real_open = dlsym(RTLD_NEXT, "open");
	if(real_open == NULL) {
		fprintf(stderr, "tup error: Unable to wrap open() function for OSX.\n");
		exit(1);
	}
}

int open(const char *pathname, int flags, ...)
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
	}
	return real_open(pathname, flags, mode);
}
