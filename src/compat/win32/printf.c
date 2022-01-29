/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2010-2022  Mike Shal <marfey@gmail.com>
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
#include <windows.h>

int __wrap___mingw_vprintf(const char *format, va_list ap) ATTRIBUTE_USED;
int __wrap___mingw_vfprintf(FILE *stream, const char *format, va_list ap) ATTRIBUTE_USED;
int __real___mingw_vfprintf(FILE *stream, const char *format, va_list ap);

static char * handle_color(HANDLE output, char *p)
{
	enum { fmask = FOREGROUND_BLUE + FOREGROUND_GREEN + FOREGROUND_RED + FOREGROUND_INTENSITY };
	enum { bmask = BACKGROUND_BLUE + BACKGROUND_GREEN + BACKGROUND_RED + BACKGROUND_INTENSITY };
	static DWORD color = FOREGROUND_BLUE + FOREGROUND_GREEN + FOREGROUND_RED;
	static int reverted = 0;

	int v = 0;
	while(*p >= '0' && *p <= '9') {
		v = v * 10 + *p++ - '0';
	}

	switch(v) {
	case 0:
		reverted = 0;
		color = FOREGROUND_BLUE + FOREGROUND_GREEN + FOREGROUND_RED;
		break;
	case 7:
		reverted = 1;
		break;
	case 31:
		color = (color & ~fmask) | FOREGROUND_RED | FOREGROUND_INTENSITY;
		break;
	case 32:
		color = (color & ~fmask) | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
		break;
	case 33:
		color = (color & ~fmask) | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
		break;
	case 34:
		color = (color & ~fmask) | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
		break;
	case 35:
		color = (color & ~fmask) | FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
		break;
	case 36:
		color = (color & ~fmask) | FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
		break;
	case 37:
		color = (color & ~fmask) | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
		break;
	case 41:
		color = (color & ~bmask) | BACKGROUND_RED | BACKGROUND_INTENSITY;
		break;
	default:;
	}

	SetConsoleTextAttribute(
		output,
		reverted? ((color & fmask) << 4) | ((color & bmask) >> 4) : color);
	return p;
}

static void parse(HANDLE output, char *p)
{
	char *out = p;
	DWORD dummy;
	while(*p) {
		if(*p++ != '' )
			continue;

		WriteConsoleA(output, out, p - 1 - out, &dummy, NULL);

		if(*p == '[')
			p++;

		p = handle_color(output, p);

		while(*p == ';') {
			p++;
			p = handle_color(output, p);
		}

		if(*p == 'm')
			p++;

		out = p;
	}

	WriteConsoleA(output, out, p - out, &dummy, NULL);
}

int __wrap___mingw_vprintf(const char *format, va_list ap)
{
	return __wrap___mingw_vfprintf(stdout, format, ap);
}

int __wrap___mingw_vfprintf(FILE *stream, const char *format, va_list ap)
{
	int rc;
	DWORD dummy;
	union {
		HANDLE h;
		intptr_t v;
	} handle;
	static int inited = 0;
	if(!inited) {
		SetConsoleOutputCP(CP_UTF8);
		inited = 1;
	}
	handle.v = _get_osfhandle(fileno(stream));
	if(GetConsoleMode(handle.h, &dummy)) {
		char buf[32 * 1024];
		rc = vsnprintf(buf, sizeof(buf), format, ap);
		parse(handle.h, buf);
		return rc;
	} else {
		rc = __real___mingw_vfprintf(stream, format, ap);
	}

	fflush(stream);
	return rc;
}
