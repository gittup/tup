/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2010  James McKaskill
 * Copyright (C) 2010-2014  Mike Shal <marfey@gmail.com>
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
#ifndef NDEBUG
#include "trace.h"
#include <stdio.h>
#include <stdarg.h>
#include <windows.h>

const char* access_type_name[] = {
	"read",
	"write",
	"rename",
	"unlink",
	"var",
};

FILE *debugf = NULL;
int opening = 0;

void debug_hook(const char* format, ...)
{
	DWORD save_error = GetLastError();

	char buf[256];
	va_list ap;
	if(debugf == NULL && !opening) {
		opening = 1;
		debugf = fopen("ok.txt", "a");
		fflush(stdout);
	}
	if(debugf == NULL) {
		printf("No file :(\n");
		goto exit;
	}
	va_start(ap, format);
	vsnprintf(buf, 255, format, ap);
	buf[255] = '\0';
	fprintf(debugf, buf);
	fflush(debugf);

exit:
	SetLastError( save_error );
}
#endif
