/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2023  Mike Shal <marfey@gmail.com>
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

#include "if_stmt.h"
#include <stdio.h>

#define IFMAX 0x80

void if_init(struct if_stmt *ifs)
{
	ifs->ifness = 0;
	ifs->level = 0;
}

int if_add(struct if_stmt *ifs, int is_true)
{
	if(ifs->level >= IFMAX) {
		fprintf(stderr, "Parse error: too many nested if statements\n");
		return -1;
	}
	if(ifs->level == 0)
		ifs->level = 1;
	else
		ifs->level <<= 1;
	if(!is_true)
		ifs->ifness |= ifs->level;
	return 0;
}

int if_else(struct if_stmt *ifs)
{
	if(ifs->level == 0) {
		fprintf(stderr, "Parse error: else statement outside of an if block\n");
		return -2;
	}
	ifs->ifness ^= ifs->level;
	return 0;
}

int if_endif(struct if_stmt *ifs)
{
	if(ifs->level == 0) {
		fprintf(stderr, "Parse error: endif statement outside of an if block\n");
		return -2;
	}
	ifs->ifness &= ~ifs->level;
	ifs->level >>= 1;
	return 0;
}

int if_true(struct if_stmt *ifs)
{
	if(ifs->ifness)
		return 0;
	return 1;
}

int if_check(struct if_stmt *ifs)
{
	if(ifs->level == 0)
		return 0;
	return -1;
}
