/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2012-2014  Mike Shal <marfey@gmail.com>
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

#define _GNU_SOURCE
#include "compat/utimensat.h"
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/syscall.h>

/* Older glibc ( < 2.6) doesn't have utimensat, but we can wrap it pretty
 * easily with a lesser accurate futimesat
 */
int utimensat(int dfd, const char *pathname, const struct timespec times[2], int flags)
{
	struct timeval tvs[2];
	if(flags) {}

#if __NR_utimensat
	if(syscall(__NR_utimensat, dfd, pathname, times, flags) == 0)
		return 0;
	/* If the syscall isn't supported, fallback to futimesat */
	if(errno != ENOSYS)
		return -1;
#endif
	tvs[0].tv_sec = times[0].tv_sec;
	tvs[0].tv_usec = times[0].tv_nsec / 1000;
	tvs[1].tv_sec = times[1].tv_sec;
	tvs[1].tv_usec = times[1].tv_nsec / 1000;

	return futimesat(dfd, pathname, tvs);
}
