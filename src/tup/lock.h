/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2023  Mike Shal <marfey@gmail.com>
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

#ifndef tup_lock_h
#define tup_lock_h

#include "tup_lock_t.h"

/** Tri-lock */
#define TUP_SHARED_LOCK ".tup/shared"
#define TUP_OBJECT_LOCK ".tup/object"
#define TUP_TRI_LOCK ".tup/tri"

/** Initializes the shared tup object lock. This allows multiple readers to
 * access tup. If you want exclusive access, you'll need to up the lock
 * to LOCK_EX by getting the file descriptor from tup_obj_lock().
 */
int tup_lock_init(void);

/** Unlocks the object lock and closes the file descriptor. It seems if the
 * OS is left to clean up the lock, it issues a close event before the lock
 * actually becomes available again.
 */
void tup_lock_exit(void);

/** Just closes the locks. This should by called by any forked processes. */
void tup_lock_closeall(void);

/* Tri-lock functions */
tup_lock_t tup_sh_lock(void);
tup_lock_t tup_obj_lock(void);
tup_lock_t tup_tri_lock(void);

#endif
