/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2012  Mike Shal <marfey@gmail.com>
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
#include <sys/time.h>
#include <sys/syscall.h>

/* Older glibc ( < 2.6) doesn't have utimensat, but we can wrap it pretty
 * easily with a lesser accurate futimesat
 */
int utimensat(int dfd, const char *pathname, const struct timespec times[2], int flags)
{
#if __NR_utimensat
	return syscall(__NR_utimensat, dfd, pathname, times, flags);
#else
	struct timeval {
		time_t tv_sec;
		long tv_usec;
	} tvs[2];
	if(flags) {}
	tvs[0].tv_sec = times[0].tv_sec;
	tvs[0].tv_usec = times[0].tv_nsec / 1000;
	tvs[1].tv_sec = times[1].tv_sec;
	tvs[1].tv_usec = times[1].tv_nsec / 1000;

	return futimesat(dfd, pathname, tvs);
#endif
}
