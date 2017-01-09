/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2017  Mike Shal <marfey@gmail.com>
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

#ifndef tup_flock_h
#define tup_flock_h

#include "tup_lock_t.h"

/* Wrappers for fcntl */
int tup_lock_open(const char *lockname, tup_lock_t *lock);
void tup_lock_close(tup_lock_t lock);
int tup_flock(tup_lock_t fd);
int tup_try_flock(tup_lock_t fd); /* Returns: -1 error, 0 got lock, 1 would block */
int tup_unflock(tup_lock_t fd);
int tup_wait_flock(tup_lock_t fd);

#endif
