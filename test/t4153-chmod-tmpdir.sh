#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2018  Mike Shal <marfey@gmail.com>
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

# Make sure the chmod() function works on a tmpdir.

. ./tup.sh

cat > ok.c << HERE
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#define mkdir(a, b) mkdir(a)
#endif

int main(void)
{
	if(mkdir("tmpdir", 0777) < 0) {
		perror("mkdir");
		return 1;
	}
	if(chmod("tmpdir", 0700) < 0) {
		perror("chmod");
		return 1;
	}
	rmdir("tmpdir");
	return 0;
}
HERE

cat > Tupfile << HERE
: ok.c |> gcc %f -o %o |> ok.exe
: ok.exe |> ./%f |>
HERE
tup touch ok.c Tupfile
update

eotup
