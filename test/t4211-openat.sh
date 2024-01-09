#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2024  Mike Shal <marfey@gmail.com>
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

# Make sure openat works properly

. ./tup.sh
check_no_windows openat

cat > prog.c << HERE
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

int main(void)
{
	int dfd;
	dfd = open("sub", O_RDONLY);
	close(openat(AT_FDCWD, "input.txt", O_RDONLY));
	close(openat(dfd, "subin.txt", O_RDONLY));
	close(openat(AT_FDCWD, "output.txt", O_WRONLY | O_CREAT, 0666));
	close(openat(dfd, "subout.txt", O_WRONLY | O_CREAT, 0666));
	return 0;
}
HERE

cat > Tupfile << HERE
: |> gcc prog.c -o %o |> prog.exe
: prog.exe |> ./prog.exe |> output.txt sub/subout.txt
HERE
mkdir sub
touch input.txt sub/subin.txt
update

tup_dep_exist . input.txt . './prog.exe'
tup_dep_exist sub subin.txt . './prog.exe'

eotup
