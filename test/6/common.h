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

#ifndef COMMON_H
#define COMMON_H

/* string.h pulls in features.h so the below define checks work */
#include <sys/time.h>
#include <string.h>

/* Only GLIBC doesn't support strlcpy */
#ifdef __GLIBC__
#  if ! defined(__UCLIBC__) && ! defined (__dietlibc__)
size_t strlcpy (char *dst, const char *src, size_t size);
#  endif
#endif

#ifdef __linux__
void srandomdev (void);
#endif

int get_time (struct timeval *tp);
long uptime (void);
void *xmalloc (size_t size);
char *xstrdup (const char *str);

#endif
