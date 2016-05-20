#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2016  Mike Shal <marfey@gmail.com>
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

# Make sure we get EMFILE or ENFILE if we hit the max file descriptors.

. ./tup.sh

cat > ok.c << HERE
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

int main(void)
{
	int fd;
	char buf[100];
	while(1) {
		fd = open("ok.c", O_RDONLY);
		if(fd < 0) {
			if(errno == EMFILE || errno == ENFILE) {
				return 0;
			}
			return -1;
		}
		if(read(fd, buf, sizeof(buf)) != sizeof(buf)) {
			perror("read");
			return -1;
		}
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
