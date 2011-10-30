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
#include <stdarg.h>

int __wrap___mingw_vprintf(const char *format, va_list ap);
int __real___mingw_vprintf(const char *format, va_list ap);

int __wrap___mingw_vfprintf(FILE *stream, const char *format, va_list ap);
int __real___mingw_vfprintf(FILE *stream, const char *format, va_list ap);

int __wrap___mingw_vprintf(const char *format, va_list ap)
{
	int rc;
	rc = __real___mingw_vprintf(format, ap);
	fflush(stdout);
	return rc;
}

int __wrap___mingw_vfprintf(FILE *stream, const char *format, va_list ap)
{
	int rc;
	rc = __real___mingw_vfprintf(stream, format, ap);
	fflush(stream);
	return rc;
}
