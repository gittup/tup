/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2014  Mike Shal <marfey@gmail.com>
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

#include "tup/access_event.h"
#include <stdio.h>
#include <fcntl.h>

void tup_send_event(const char *file, int len, const char *file2, int len2, int at)
{
	char path[PATH_MAX];

	if(file2 || len2 || at) {/*TODO */}
	if(snprintf(path, sizeof(path), TUP_VAR_VIRTUAL_DIR "/%.*s", len, file) >= (signed)sizeof(path)) {
		fprintf(stderr, "tup internal error: path is too small in tup_send_event()\n");
		return;
	}

	open(path, O_RDONLY);
}
