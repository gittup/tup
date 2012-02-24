/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2012  Mike Shal <marfey@gmail.com>
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

#include "colors.h"
#include "db_types.h"
#include "option.h"
#include <string.h>
#include <unistd.h>

static int enabled[2];
static int active = 0;
/* error_mode is set when we hightlight the whole line in red. In this case,
 * we don't want to switch to a real color when printing tup entrys
 */
static int error_mode = 0;

void color_init(void)
{
	const char *colors;
	colors = tup_option_get_string("display.color");
	if(strcmp(colors, "never") == 0) {
		enabled[0] = 0;
		enabled[1] = 0;
	} else if(strcmp(colors, "always") == 0) {
		enabled[0] = 1;
		enabled[1] = 1;
	} else {
		if(isatty(STDOUT_FILENO))
			enabled[0] = 1;
		else
			enabled[0] = 0;
		if(isatty(STDERR_FILENO))
			enabled[1] = 1;
		else
			enabled[1] = 0;
	}
}

void color_set(FILE *f)
{
	if(f == stdout)
		active = 0;
	else
		active = 1;
}

const char *color_type(int type)
{
	const char *color = "";

	if(!enabled[active] || error_mode)
		return "";

	switch(type) {
		case TUP_NODE_ROOT:
			/* Overloaded to mean a node in error (can be re-used
			 * since TUP_NODE_ROOT is never displayed).
			 */
			color = "[31";
			break;
		case TUP_NODE_DIR:
			color = "[33";
			break;
		case TUP_NODE_CMD:
			color = "[34";
			break;
		case TUP_NODE_GENERATED:
			color = "[35";
			break;
		case TUP_NODE_FILE:
			/* If a generated node becomes a normal file (t6031) */
			color = "[37";
			break;
		case TUP_NODE_GHOST:
			/* Used for reporting external file modifications */
			color="[0";
			break;
	}
	return color;
}

const char *color_append_normal(void)
{
	if(!enabled[active] || error_mode)
		return "";
	return "m";
}

const char *color_append_reverse(void)
{
	if(!enabled[active])
		return "";
	return ";07m";
}

const char *color_reverse(void)
{
	if(!enabled[active])
		return "";
	return "[07m";
}

const char *color_end(void)
{
	if(!enabled[active])
		return "";
	return "[0m";
}

const char *color_final(void)
{
	if(!enabled[active])
		return "";
	return "[07;32m";
}

const char *color_error_mode(void)
{
	if(!enabled[active])
		return "";
	error_mode = 1;
	return "[41;37m";
}

void color_error_mode_clear(void)
{
	error_mode = 0;
}
