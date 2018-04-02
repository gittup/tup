/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2018  Mike Shal <marfey@gmail.com>
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

#ifndef tup_access_event_h
#define tup_access_event_h

#include "compat.h"

/** The filename to write file accesses to. */
#define TUP_DEPFILE "TUP_DEPFILE"

/** The file descriptor for the variable dictionary. */
#define TUP_VARDICT_NAME "tup_vardict"

/* The virtual directory used to pass @-variable dependencies from a client
 * program to the server.
 */
#define TUP_VAR_VIRTUAL_DIR "@tup@"
#define TUP_VAR_VIRTUAL_DIR_LEN (sizeof(TUP_VAR_VIRTUAL_DIR)-1)

enum access_type {
	ACCESS_READ,
	ACCESS_WRITE,
	ACCESS_RENAME,
	ACCESS_UNLINK,
	ACCESS_VAR,
};

/** Structure sent across the unix socket to notify the main wrapper of any
 * file accesses.
 */
struct access_event {
	/** This field must always be set to one of the values in the
	 * access_type enum.
	 */
	enum access_type at;

	/** Length of the path, which will follow the access_event struct */
	int len;

	/** Length of the second path, for events that require two paths */
	int len2;
};

/* Windows' wider path max */
#define WIDE_PATH_MAX 32767
#define ACCESS_EVENT_MAX_SIZE (WIDE_PATH_MAX * 2 + sizeof(struct access_event))
void tup_send_event(const char *file, int len, const char *file2, int len2, int at);

#endif
