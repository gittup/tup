/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2010-2017  Mike Shal <marfey@gmail.com>
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

#include "dirpath.h"

int __wrap_dup(int oldfd) ATTRIBUTE_USED;
int __real_dup(int oldfd);

int __wrap_dup(int oldfd)
{
	int rc;

	rc = win32_dup(oldfd);
	/* -1 means we should've win32_dup'd it but it failed */
	if(rc == -1)
		return -1;
	if(rc > 0)
		return rc;
	return __real_dup(oldfd);
}