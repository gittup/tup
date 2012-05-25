/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2010-2012  Mike Shal <marfey@gmail.com>
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

#include <stddef.h> /* get size_t */
#include <fcntl.h> /* get mode_t */
#define AT_SYMLINK_NOFOLLOW 0x100

struct stat;

int fchdir(int fd);
int fstatat(int dirfd, const char *pathname, struct stat *buf, int flags);
int mkstemp(char *template);
int openat(int dirfd, const char *pathname, int flags, ...);
int unlinkat(int dirfd, const char *pathname, int flags);
int readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz);
int mkdirat(int dirfd, const char *pathname, mode_t mode);
int renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath);
int symlink(const char *oldpath, const char *newpath);
