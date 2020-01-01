/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2020  Mike Shal <marfey@gmail.com>
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

#ifndef tup_if_h
#define tup_if_h

struct if_stmt {
	unsigned char ifness;
	unsigned char level;
};

void if_init(struct if_stmt *ifs);
int if_add(struct if_stmt *ifs, int is_true);
int if_else(struct if_stmt *ifs);
int if_endif(struct if_stmt *ifs);
int if_true(struct if_stmt *ifs);
int if_check(struct if_stmt *ifs);

#endif
