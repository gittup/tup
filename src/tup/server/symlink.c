/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2022  Mike Shal <marfey@gmail.com>
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

#include "tup/server.h"
#include "tup/file.h"
#include "tup/estring.h"
#include "tup/variant.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int server_symlink(struct server *s, struct tup_entry *dtent, const char *target, int dfd, const char *linkpath)
{
	const char *symtarget;
	int is_variant = tup_entry_variant(dtent)->root_variant;
	struct estring e;

	if(is_variant) {
		symtarget = target;
	} else {
		if(estring_init(&e) < 0)
			return -1;
		if(get_relative_dir(NULL, &e, dtent->tnode.tupid, variant_tent_to_srctent(dtent)->tnode.tupid) < 0)
			return -1;
		estring_append(&e, "/", 1);
		estring_append(&e, target, strlen(target));
		symtarget = e.s;
	}
	if(symlinkat(symtarget, dfd, linkpath) < 0) {
		perror("symlinkat");
		fprintf(stderr, "tup error: unable to create symlink at '%s' pointing to target '%s'\n", linkpath, target);
		return -1;
	}
	if(handle_file_dtent(ACCESS_WRITE, variant_tent_to_srctent(dtent), linkpath, &s->finfo)< 0)
		return -1;
	if(!is_variant) {
		free(e.s);
	}
	return 0;
}
