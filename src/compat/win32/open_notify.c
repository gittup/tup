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

#include "open_notify.h"
#include "bsd/queue.h"
#include "tup/file.h"
#include "tup/db.h"
#include "tup/config.h"
#include <stdio.h>
#include <sys/stat.h>

struct finfo_list {
	LIST_ENTRY(finfo_list) list;
	struct file_info *finfo;
};
LIST_HEAD(,finfo_list) finfo_list_head;

int open_notify_push(struct file_info *finfo)
{
	struct finfo_list *flist;

	flist = malloc(sizeof *flist);
	if(!flist) {
		perror("malloc");
		return -1;
	}
	flist->finfo = finfo;
	LIST_INSERT_HEAD(&finfo_list_head, flist, list);
	return 0;
}

int open_notify_pop(struct file_info *finfo)
{
	struct finfo_list *flist;
	if(LIST_EMPTY(&finfo_list_head)) {
		fprintf(stderr, "tup internal error: finfo_list is empty.\n");
		return -1;
	}
	flist = LIST_FIRST(&finfo_list_head);
	if(flist->finfo != finfo) {
		fprintf(stderr, "tup internal error: open_notify_pop() element is not at the head of the list.\n");
		return -1;
	}
	LIST_REMOVE(flist, list);
	free(flist);
	return 0;
}

int open_notify(enum access_type at, const char *pathname)
{
	/* For the parser: manually keep track of file accesses, since we
	 * don't run the UDP server for the parsing stage in win32.
	 */
	if(!LIST_EMPTY(&finfo_list_head)) {
		struct finfo_list *flist;
		struct stat buf;
		char fullpath[PATH_MAX];
		int cwdlen;
		int pathlen;

		if(getcwd(fullpath, sizeof(fullpath)) != fullpath) {
			perror("getcwd");
			return -1;
		}
		cwdlen = strlen(fullpath);
		pathlen = strlen(pathname);
		if(cwdlen + pathlen + 2 >= (signed)sizeof(fullpath)) {
			fprintf(stderr, "tup internal error: max pathname exceeded.\n");
			return -1;
		}
		fullpath[cwdlen] = path_sep();
		memcpy(fullpath + cwdlen + 1, pathname, pathlen);
		fullpath[cwdlen + pathlen + 1] = 0;

		/* If the stat fails, or if the stat works and we know it
		 * is a directory, don't actually add the dependency. We
		 * want failed stats for ghost nodes, and all successful
		 * file accesses.
		 */
		if(stat(pathname, &buf) < 0 || !S_ISDIR(buf.st_mode)) {
			flist = LIST_FIRST(&finfo_list_head);
			if(handle_open_file(at, fullpath, flist->finfo) < 0)
				return -1;
		}
	}
	return 0;
}
