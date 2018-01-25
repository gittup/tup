/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2018  Mike Shal <marfey@gmail.com>
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

#ifndef tup_option_h
#define tup_option_h

int tup_option_process_ini(void);
int tup_option_init(int argc, char **argv);
void tup_option_exit(void);
int tup_option_get_int(const char *opt);
int tup_option_get_flag(const char *opt);
const char *tup_option_get_string(const char *opt);
const char *tup_option_get_location(const char *opt);
int tup_option_show(void);

#define TUP_OPTIONS_FILE ".tup/options"

#endif
