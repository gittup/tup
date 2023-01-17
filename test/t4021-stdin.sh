#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2023  Mike Shal <marfey@gmail.com>
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

# Make sure a command that tries to read from stdin won't hang tup.

. ./tup.sh

cat > ok.c << HERE
#include <stdio.h>
#include <unistd.h>

int main(void)
{
	int i;
	char c = 0;
	i = read(0, &c, 1);
	if(i > 0) {
		fprintf(stderr, "Error: read test should return 0 or -1\n");
		return 1;
	}
	if(c != 0) {
		fprintf(stderr, "Error: char 'c' should not be set.\n");
		return 1;
	}
	return 0;
}
HERE
cat > Tupfile << HERE
: ok.c |> gcc %f -o %o |> read_test.exe
: read_test.exe |> ./%f |>
HERE
update

eotup
