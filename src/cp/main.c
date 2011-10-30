/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2010  James McKaskill
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

#include <windows.h>
#include <stdio.h>
#include <errno.h>

int main(int argc, char* argv[])
{
	int i;

	if (argc < 3) {
		fprintf(stderr, "tup-cp [sources...] [destination]\n");
		exit(-1);
	}

	for (i = 1; i < argc - 1; i++) {
		if (!CopyFileA(argv[i], argv[argc-1], FALSE)) {
			perror("copy");
		}
	}

	return 0;
}
