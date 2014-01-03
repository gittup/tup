/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2009-2014  Mike Shal <marfey@gmail.com>
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
#include <string.h>
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
	int offset = 0;
	int read_bytes;
	int total_bytes;

	if(fstat(fd, &st) < 0) {
		perror("fstat");
		return -1;
	}

	tmp = malloc(st.st_size + extra);
	if(!tmp) {
		perror("malloc");
		return -1;
	}

	read_bytes = st.st_size;
	total_bytes = st.st_size;
	if(read_bytes >= 3) {
		unsigned char bom[3];
		rc = read(fd, bom, 3);
		if(rc != 3) {
			perror("fslurp: read bom");
			goto err_out;
		}

		read_bytes -= 3;

		/* If the first 3 bytes are the UTF-8 byte-order mark, just
		 * drop them from the buffer. Otherwise these are the first 3
		 * bytes of the file, so copy them over.
		 */
		if(bom[0] == 0xef && bom[1] == 0xbb && bom[2] == 0xbf) {
			total_bytes -= 3;
		} else {
			memcpy(tmp, bom, 3);
			offset = 3;
		}
	}

	rc = read(fd, tmp + offset, read_bytes);
	if(rc < 0) {
		perror("fslurp: read");
		goto err_out;
	}
	if(rc != read_bytes) {
		fprintf(stderr, "tup error: read %i bytes, but expected %ji bytes\n", rc, (intmax_t)read_bytes);
		errno = EIO;
		goto err_out;
	}

	b->s = tmp;
	b->len = total_bytes;
	return 0;

err_out:
	free(tmp);
	return -1;
}
