/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2015  Mike Shal <marfey@gmail.com>
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

#include "vardict.h"
#include "access_event.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>

struct vardict {
	unsigned int len;
	unsigned int num_entries;
	unsigned int *offsets;
	const char *entries;
	void *map;
};

static struct vardict tup_vars;

int tup_vardict_init(void)
{
	char *path;
	int fd;
	struct stat buf;
	unsigned int expected = 0;

	path = getenv(TUP_VARDICT_NAME);
	if(!path) {
		fprintf(stderr, "tup client error: Couldn't find path for '%s'\n",
			TUP_VARDICT_NAME);
		return -1;
	}
	fd = strtol(path, NULL, 0);
	if(fd <= 0) {
		/* If we don't have a vardict file, it's because there are
		 * no variables.
		 */
		tup_vars.len = 0;
		tup_vars.num_entries = 0;
		tup_vars.offsets = NULL;
		tup_vars.entries = NULL;
		tup_vars.map = NULL;
		return 0;
	}

	if(fstat(fd, &buf) < 0) {
		perror("fstat");
		return -1;
	}
	tup_vars.len = buf.st_size;
	if(tup_vars.len == 0) {
		/* Empty file is ok - no variables will be read */
		tup_vars.num_entries = 0;
		tup_vars.offsets = NULL;
		tup_vars.entries = NULL;
		tup_vars.map = NULL;
		return 0;
	}

	expected += sizeof(unsigned int);
	if(tup_vars.len < expected) {
		fprintf(stderr, "tup error: var-tree should be at least sizeof(unsigned int) bytes, but got %i bytes\n", tup_vars.len);
		return -1;
	}
	tup_vars.map = mmap(NULL, tup_vars.len, PROT_READ, MAP_PRIVATE, fd, 0);
	if(tup_vars.map == MAP_FAILED) {
		perror("mmap");
		return -1;
	}

	tup_vars.num_entries = *(unsigned int*)tup_vars.map;
	tup_vars.offsets = (unsigned int*)(void*)((char*)tup_vars.map + expected);
	expected += sizeof(unsigned int) * tup_vars.num_entries;
	tup_vars.entries = (const char*)tup_vars.map + expected;
	if(tup_vars.len < expected) {
		fprintf(stderr, "tup error: var-tree should have at least %i bytes to accommodate the index, but got %i bytes\n", expected, tup_vars.len);
		return -1;
	}

	return 0;
}

const char *tup_config_var(const char *key, int keylen)
{
	int left = -1;
	int right = tup_vars.num_entries;
	int cur;
	const char *p;
	const char *k;
	int bytesleft;

	if(keylen == -1)
		keylen = strlen(key);

	tup_send_event(key, keylen, "", 0, ACCESS_VAR);
	while(1) {
		cur = (right - left) >> 1;
		if(cur <= 0)
			break;
		cur += left;
		if(cur >= (signed)tup_vars.num_entries)
			break;

		if(tup_vars.offsets[cur] >= tup_vars.len) {
			fprintf(stderr, "tup error: Offset for element %i is out of bounds.\n", cur);
			break;
		}
		p = tup_vars.entries + tup_vars.offsets[cur];
		k = key;
		bytesleft = keylen;
		while(bytesleft > 0) {
			/* Treat '=' as if p ended */
			if(*p == '=') {
				left = cur;
				goto out_next;
			}
			if(*p < *k) {
				left = cur;
				goto out_next;
			} else if(*p > *k) {
				right = cur;
				goto out_next;
			}
			p++;
			k++;
			bytesleft--;
		}

		if(*p != '=') {
			right = cur;
			goto out_next;
		}
		return p+1;
out_next:
		;
	}
	return NULL;
}
