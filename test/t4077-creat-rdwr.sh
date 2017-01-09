#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2017  Mike Shal <marfey@gmail.com>
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

# Make sure we work with 'sed -i', which has an unusual temporary file creation
# characteristic - it creates a file with mode 000 so it can't be opened by
# other processes (I guess?). In fuse, if create() is not implemented, it
# mimics it by doing mknod() and then open(). Unfortunately, if the mknod()
# uses mode 000, then the open() fails. Therefore, tup needs to implement
# create() so this can be done in a single call.

. ./tup.sh

# Make sure we can creat() a file in RDWR mode. Here we use fopen() and try to
# read from it with fscanf().
cat > ok.c << HERE
#include <stdio.h>
#include <fcntl.h>

int main(void)
{
	int x;
	FILE *f = fopen("out.txt", "w+");
	fprintf(f, "5\n");
	rewind(f);
	if(fscanf(f, "%i", &x) != 1) {
		perror("fscanf");
		return 1;
	}
	fclose(f);
	if(x != 5)
		return 1;
	return 0;
}
HERE
cat > Tupfile << HERE
: |> gcc ok.c -o %o |> ok.exe
: ok.exe |> ./%f |> out.txt
HERE
tup touch ok.c Tupfile
update

eotup
