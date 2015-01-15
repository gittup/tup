/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2015  Mike Shal <marfey@gmail.com>
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

#include "debug.h"
#include <stdio.h>

static int debugging = 0;
static int server_debug = 0;
static const char *dstring = NULL;

int debug_enabled(void)
{
	return debugging;
}

const char *debug_string(void)
{
	return dstring;
}

void debug_enable(const char *label)
{
	debugging = 1;
	dstring = label;
}

void debug_disable(void)
{
	debugging = 0;
}

void server_enable_debug(void)
{
	server_debug = 1;
}

int server_debug_enabled(void)
{
	return server_debug;
}
