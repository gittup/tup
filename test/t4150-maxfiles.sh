#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013  Mike Shal <marfey@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

# Try to hit the file-system limit, and make sure we can close the last
# fd and then open a new one.

. ./tup.sh

cat > ok.c << HERE
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

int main(void)
{
	int fd;
	int lastfd;
	while(1) {
		fd = open("ok.c", O_RDONLY);
		if(fd < 0) {
			if(errno == EMFILE || errno == ENFILE) {
				if(close(lastfd) < 0) {
					perror("close");
					return -1;
				}
				fd = open("ok.c", O_RDONLY);
				if(fd < 0) {
					perror("ok.c - last");
					return -1;
				}
				break;
			}
			perror("ok.c");
			fprintf(stderr, "Can't open ok.c\\n");
			return -1;
		}
		lastfd = fd;
	}
	return 0;
}
HERE
cat > Tupfile << HERE
: |> gcc ok.c -o %o |> prog.exe
: prog.exe |> ./%f |>
HERE
update

eotup
