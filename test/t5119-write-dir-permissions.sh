#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2023-2024  Mike Shal <marfey@gmail.com>
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

# Make sure trying to write to a file in a directory that we don't have
# permissions to write to fails.

. ./tup.sh
check_tup_suid

set_full_deps

cat > Tupfile << HERE
: |> gcc ok.c -o %o |> ok.exe
: ok.exe |> ./ok.exe |> good.txt
HERE

cat > ok.c << HERE
#include <stdio.h>
int main(void)
{
	FILE *f = fopen("/usr/tuptest.txt", "w");
	if(!f) {
		f = fopen("good.txt", "w");
	}
	fclose(f);
	return 0;
}
HERE
update

eotup
