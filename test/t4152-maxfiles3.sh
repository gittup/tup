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
# fd and then create a new file and write to it.

. ./tup.sh
check_no_windows mmap

cat > ok.c << HERE
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>

int main(void)
{
	int fd;
	int lastfd;
	int lastfd2;
	void *map;
	while(1) {
		fd = open("ok.c", O_RDONLY);
		if(fd < 0) {
			if(errno == EMFILE || errno == ENFILE) {
				char buf[4];
				if(close(lastfd) < 0) {
					perror("close");
					return -1;
				}
				if(close(lastfd2) < 0) {
					perror("close2");
					return -1;
				}
				fd = creat("new.txt", 0666);
				if(fd < 0) {
					perror("new.txt");
					return -1;
				}
				write(fd, "hey\\n", 4);
				close(fd);
				/* Open twice to trigger the mapping
				* logic.
				*/
				fd = open("new.txt", O_RDONLY);
				fd = open("new.txt", O_RDONLY);
				if(fd < 0) {
					perror("new.txt - read");
					return -1;
				}
				if(read(fd, buf, 4) < 0) {
					perror("read");
					return -1;
				}
				if(memcmp(buf, "hey\\n", 4) != 0) {
					fprintf(stderr, "Expected 'hey'\\n");
				}
				break;
			}
			perror("ok.c");
			fprintf(stderr, "Can't open ok.c\\n");
			return -1;
		}
		lastfd2 = lastfd;
		lastfd = fd;
		map = mmap(NULL, 5, PROT_READ, MAP_SHARED, fd, 0);
	}
	return 0;
}
HERE
cat > Tupfile << HERE
: |> gcc ok.c -o %o |> prog.exe
: prog.exe |> ./%f |> new.txt
HERE
update

echo "hey" | diff - new.txt

eotup
