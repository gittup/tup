/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2013-2024  Mike Shal <marfey@gmail.com>
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

#include "estring.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ESTRING_DEFAULT_SIZE 4096

int estring_init(struct estring *e)
{
	e->len = 0;
	e->maxlen = ESTRING_DEFAULT_SIZE;
	e->s = malloc(e->maxlen);
	if(!e->s) {
		perror("malloc");
		return -1;
	}
	e->s[0] = 0;
	return 0;
}

int estring_append(struct estring *e, const char *src, int len)
{
	if(e->len + len + 1 > e->maxlen) {
		while(e->len + len + 1 > e->maxlen) {
			e->maxlen *= 2;
		}
		e->s = realloc(e->s, e->maxlen);
		if(!e->s) {
			perror("realloc");
			return -1;
		}
	}
	memcpy(e->s + e->len, src, len);
	e->len += len;
	e->s[e->len] = 0;
	return 0;
}

int estring_append_escape(struct estring *e, const char *src, int len, char escape)
{
	const char *p = src;
	const char *endp = src + len;
	while(p < endp) {
		const char *next;
		next = memchr(p, escape, endp - p);
		if(!next) {
			return estring_append(e, p, endp - p);
		}
		if(estring_append(e, p, next - p) < 0)
			return -1;
		if(escape == '\'') {
			if(estring_append(e, "'\"'\"'", 5) < 0)
				return -1;
		} else {
			if(estring_append(e, "\\\"", 2) < 0)
				return -1;
		}
		p = next + 1;
	}
	return 0;
}
