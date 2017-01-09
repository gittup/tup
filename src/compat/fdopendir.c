/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2010-2017  Mike Shal <marfey@gmail.com>
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
#include <sys/param.h>
#include <sys/fcntl.h>
#include <dirent.h>
#include <unistd.h>

DIR *fdopendir(int fd)
{
#ifdef __APPLE__
	char fullpath[MAXPATHLEN];
	DIR *d;

	if(fcntl(fd, F_GETPATH, fullpath) < 0) {
		perror("fcntl");
		fprintf(stderr, "tup error: Unable to convert file descriptor back to pathname in fdopendir() compat library.\n");
		return NULL;
	}
	if(close(fd) < 0) {
		perror("close(fd) in tup's OSX fdopendir() wrapper:");
		return NULL;
	}

	d = opendir(fullpath);
	return d;
#else
#error Unsupported platform in fdopendir.c compat library
#endif
}
