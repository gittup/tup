#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2021  Mike Shal <marfey@gmail.com>
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

# Make sure stat() creates a dependency, whether or not it returns ENOENT. It
# seems we never had an explicit test for this. The original LD_PRELOAD
# implementation only counted stat() calls that returned ENOENT, and assumed
# that if it returned success, the program would then open() the file and we
# would count the open() instead. However, clang may call stat() on a directory
# passed in via -I, and if it is a normal file, it will not look for include
# files there. This matters for cases where the -I path is also a generated
# file, which should always be an error, regardless of whether or not the file
# already exists.

. ./tup.sh

cat > Tupfile << HERE
: stat.c |> gcc %f -o %o |> stat.exe
: stat.exe |> ./%f |>
HERE
cat > stat.c << HERE
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void)
{
	struct stat buf;
	if(stat("tmp.txt", &buf) < 0) {
		perror("tmp.txt");
		return 1;
	}
	if(stat("ghost.txt", &buf) == 0) {
		fprintf(stderr, "Error: expected stat on ghost.txt to fail.\\n");
		return 1;
	}
	return 0;
}
HERE
touch tmp.txt
update

tup_dep_exist . tmp.txt . ./stat.exe
tup_dep_exist . ghost.txt . ./stat.exe

eotup
