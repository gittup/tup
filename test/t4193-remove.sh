#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2023  Mike Shal <marfey@gmail.com>
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

# Make sure the remove() call works just like unlink()

. ./tup.sh

cat > Tupfile << HERE
: remove.c |> gcc %f -o %o |> remove.exe
: remove.exe |> ./%f |>
HERE
cat > remove.c << HERE
#include <stdio.h>

int main(void)
{
	FILE *f;
	f = fopen("temp.txt", "w+");
	if(!f) {
		perror("temp.txt");
		return 1;
	}
	fclose(f);
	if(remove("temp.txt") < 0) {
		perror("remove");
		return 1;
	}
	return 0;
}
HERE
update

eotup
