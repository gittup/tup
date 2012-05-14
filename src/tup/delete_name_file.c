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

#define _ATFILE_SOURCE
#include "fileio.h"
#include "db.h"
#include "entry.h"
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

int delete_name_file(tupid_t tupid)
{
	if(tup_db_unflag_config(tupid) < 0)
		return -1;
	if(tup_db_unflag_create(tupid) < 0)
		return -1;
	if(tup_db_unflag_modify(tupid) < 0)
		return -1;
	if(tup_db_unflag_variant(tupid, 1) < 0)
		return -1;
	if(tup_db_delete_links(tupid) < 0)
		return -1;
	if(tup_db_delete_node(tupid) < 0)
		return -1;
	return 0;
}

int delete_file(tupid_t dt, const char *name)
{
	int dirfd;
	int rc = 0;

	dirfd = tup_entry_open_tupid(dt);
	if(dirfd < 0) {
		if(dirfd == -ENOENT) {
			/* If the directory doesn't exist, the file can't
			 * either
			 */
			return 0;
		} else {
			return -1;
		}
	}

	if(unlinkat(dirfd, name, 0) < 0) {
		/* Don't care if the file is already gone, or if the name
		 * is too long to exist in the filesystem anyway.
		 */
		if(errno != ENOENT && errno != ENAMETOOLONG) {
			perror(name);
			rc = -1;
			goto out;
		}
	}

out:
	if(close(dirfd) < 0) {
		perror("close(dirfd)");
		return -1;
	}
	return rc;
}
