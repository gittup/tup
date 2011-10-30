/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2009-2011  Mike Shal <marfey@gmail.com>
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

#include "fslurp.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/stat.h>

static int do_slurp(int fd, struct buf *b, int extra);

int fslurp(int fd, struct buf *b)
{
	return do_slurp(fd, b, 0);
}

int fslurp_null(int fd, struct buf *b)
{
	int rc;

	rc = do_slurp(fd, b, 1);
	if(rc == 0)
		b->s[b->len] = 0;
	return rc;
}

static int do_slurp(int fd, struct buf *b, int extra)
{
	struct stat st;
	char *tmp;
	int rc;

	if(fstat(fd, &st) < 0) {
		perror("fstat");
		return -1;
	}

	tmp = malloc(st.st_size + extra);
	if(!tmp) {
		perror("malloc");
		return -1;
	}

	rc = read(fd, tmp, st.st_size);
	if(rc < 0) {
		perror("read");
		goto err_out;
	}
	if(rc != st.st_size) {
		fprintf(stderr, "tup error: read %i bytes, but expected %ji bytes\n", rc, (intmax_t)st.st_size);
		errno = EIO;
		goto err_out;
	}

	b->s = tmp;
	b->len = st.st_size;
	return 0;

err_out:
	free(tmp);
	return -1;
}
