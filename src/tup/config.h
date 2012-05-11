/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2012  Mike Shal <marfey@gmail.com>
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

#ifndef tup_config_h
#define tup_config_h

#include "tupid.h"

int find_tup_dir(void);
int open_tup_top(void);
tupid_t get_sub_dir_dt(void);
const char *get_tup_top(void);
int get_tup_top_len(void);
const char *get_sub_dir(void);
int get_sub_dir_len(void);
int tup_top_fd(void);
int display_output(int fd, int iserr, const char *name, int display_name);

#endif
