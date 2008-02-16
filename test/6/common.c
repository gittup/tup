/*
 * dhcpcd - DHCP client daemon -
 * Copyright 2006-2007 Roy Marples <uberlord@gentoo.org>
 * 
 * dhcpcd is an RFC2131 compliant DHCP client daemon.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <sys/time.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "logger.h"

/* OK, this should be in dhcpcd.c
 * It's here to make dhcpcd more readable */
#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>
void srandomdev (void) {
	int fd;
	unsigned long seed;

	fd = open ("/dev/urandom", 0);
	if (fd == -1 || read (fd,  &seed, sizeof(seed)) == -1) {
		logger (LOG_WARNING, "Could not load seed from /dev/urandom: %s",
				strerror (errno));
		seed = time (0);
	}
	if (fd >= 0)
		close(fd);

	srandom (seed);
}
#endif

/* strlcpy is nice, shame glibc does not define it */
#ifdef __GLIBC__
#  if ! defined (__UCLIBC__) && ! defined (__dietlibc__)
size_t strlcpy (char *dst, const char *src, size_t size)
{
	const char *s = src;
	size_t n = size;

	if (n && --n)
		do {
			if (! (*dst++ = *src++))
				break;
		} while (--n);

	if (! n) {
		if (size)
			*dst = '\0';
		while (*src++)
			;
	}

	return (src - s - 1);
}
#  endif
#endif

/* This requires us to link to rt on glibc, so we use sysinfo instead */
#ifdef __linux__
#include <sys/sysinfo.h>
/* Avoid clock_gettime as it requires librt on Linux */
#undef CLOCK_MONOTONIC
long uptime (void)
{
	struct sysinfo info;

	sysinfo (&info);
	return info.uptime;
}
#elif CLOCK_MONOTONIC
long uptime (void)
{
	struct timespec tp;

	if (clock_gettime (CLOCK_MONOTONIC, &tp) == -1) {
		logger (LOG_ERR, "clock_gettime: %s", strerror (errno));
		return -1;
	}

	return tp.tv_sec;
}
#else
/* Darwin doesn't appear to have an uptime, so try and make one ourselves */
long uptime (void)
{
	struct timeval tv;
	static long start = 0;

	if (gettimeofday (&tv, NULL) == -1) {
		logger (LOG_ERR, "gettimeofday: %s", strerror (errno));
		return -1;
	}

	if (start == 0)
		start = tv.tv_sec;

	return tv.tv_sec - start;
}
#endif

/* Handy function to get the time.
 * We only care about time advancements, not the actual time itself
 * Which is why we use CLOCK_MONOTONIC, but it is not available on all
 * platforms.
 */
#ifdef CLOCK_MONOTONIC 
int get_time (struct timeval *tp)
{
	struct timespec ts;

	if (clock_gettime (CLOCK_MONOTONIC, &ts) == -1) {
		logger (LOG_ERR, "clock_gettime: %s", strerror (errno));
		return (-1);
	}

	tp->tv_sec = ts.tv_sec;
	tp->tv_usec = ts.tv_nsec / 1000;
	return (0);
}
#else
int get_time (struct timeval *tp)
{
	if (gettimeofday (tp, NULL) == -1) {
		logger (LOG_ERR, "gettimeofday: %s", strerror (errno));
		return (-1);
	}
	return (0);
}
#endif

void *xmalloc (size_t s)
{
	void *value = malloc (s);

	if (value)
		return (value);

	logger (LOG_ERR, "memory exhausted");
	exit (EXIT_FAILURE);
}

char *xstrdup (const char *str)
{
	char *value;

	if (! str)
		return (NULL);

	if ((value = strdup (str)))
		return (value);

	logger (LOG_ERR, "memory exhausted");
	exit (EXIT_FAILURE);
}

