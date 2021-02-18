/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2010-2021  Mike Shal <marfey@gmail.com>
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

#include "platform.h"

/* NOTE: Please keep the list in tup.1 in sync */
#ifdef __linux__
const char *tup_platform = "linux";
#elif __sun__
const char *tup_platform = "solaris";
#elif __APPLE__
const char *tup_platform = "macosx";
#elif _WIN32
const char *tup_platform = "win32";
#elif __FreeBSD__
const char *tup_platform = "freebsd";
#elif defined(__NetBSD__)
const char *tup_platform = "netbsd";
#else
#error Unsupported platform. Please add support in tup/platform.c
#endif


/* NOTE: Please keep the list in tup.1 in sync */

#ifdef __x86_64__
const char *tup_arch = "x86_64";
#elif __i386__
const char *tup_arch = "i386";
#elif __powerpc__
const char *tup_arch = "powerpc";
#elif __powerpc64__
const char *tup_arch = "powerpc64";
#elif __ia64__
const char *tup_arch = "ia64";
#elif __alpha__
const char *tup_arch = "alpha";
#elif __sparc__
const char *tup_arch = "sparc";
#elif __arm__
const char *tup_arch = "arm";
#elif __aarch64__
const char *tup_arch = "arm64";
#else
#error Unsupported cpu architecture. Please add support in tup/platform.c
#endif
