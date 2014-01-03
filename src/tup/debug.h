/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2014  Mike Shal <marfey@gmail.com>
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

#ifndef tup_debug_h
#define tup_debug_h

#include <unistd.h>

/** Environment variable to set if debugging is enabled. */
#define TUP_DEBUG "tup_debug"
#define DEBUGP(format, args...) if(debug_enabled()) fprintf(stderr, "[34m[%s:%i %s:%i][0m " format, debug_string(), getpid(), __FILE__, __LINE__, ##args)

int debug_enabled(void);
const char *debug_string(void);
void debug_enable(const char *label);
void debug_disable(void);
void server_enable_debug(void);
int server_debug_enabled(void);

#endif
