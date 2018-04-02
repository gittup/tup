#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018  Mike Shal <marfey@gmail.com>
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

# Similar to t4190, but with a NULL buffer that should get allocated by libc.
. ./tup.sh
check_no_windows realpath

cat > Tupfile << HERE
: |> gcc foo.c -o %o |> foo
: foo |> ./foo |>
HERE
cat > foo.c << HERE
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <fcntl.h>

int main(void)
{
	int fd;
	char *resolved_path;
	resolved_path = realpath("link.txt", NULL);
	if(resolved_path == NULL) {
		perror("realpath");
		return 1;
	}
	fd = open(resolved_path, O_RDONLY);
	if(fd < 0) {
		perror(resolved_path);
		return 1;
	}
	return 0;
}
HERE

touch bar.txt
ln -s bar.txt link.txt
update

tup_dep_exist . bar.txt . './foo'
tup_dep_exist . link.txt . './foo'

eotup
