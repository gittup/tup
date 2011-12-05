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

#ifndef tup_compat_macosx_h
#define tup_compat_macosx_h

#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>

/* MacOSX 10.6 does not have *at() functions */
DIR *fdopendir(int fd);
int faccessat(int dirfd, const char *pathname, int mode, int flags);
int fchmodat(int dirfd, const char *pathname, mode_t mode, int flags);
int fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags);
int fstatat(int dirfd, const char *pathname, struct stat *buf, int flags);
int mkdirat(int dirfd, const char *pathname, mode_t mode);
int openat(int dirfd, const char *pathname, int flags, ...);
int readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz);
int renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath);
int symlinkat(const char *oldpath, int newdirfd, const char *newpath);
int unlinkat(int dirfd, const char *pathname, int flags);
int utimensat(int dirfd, const char *pathname, const struct timespec times[2], int flags);

/* clearenv also seems to be missing */
int clearenv(void);

#endif
