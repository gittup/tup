/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2010-2012  Mike Shal <marfey@gmail.com>
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

#ifndef tup_colors_h
#define tup_colors_h

#include <stdio.h>
#include "db_types.h"

void color_init(void);
void color_set(FILE *f);
const char *color_type(enum TUP_NODE_TYPE type);
const char *color_append_normal(void);
const char *color_append_reverse(void);
const char *color_reverse(void);
const char *color_end(void);
const char *color_final(void);
const char *color_error_mode(void);
void color_error_mode_clear(void);

#endif
