#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2012  Mike Shal <marfey@gmail.com>
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

# Tup should parse correctly if the last newline is missing.

. ./tup.sh

cat > ok.c << HERE
#include <stdio.h>

int main(void)
{
	printf(": foreach *.c |> gcc -c %%f -o %%o |> %%B.o");
	return 0;
}
HERE
gcc ok.c -o ok
./ok > Tupfile

tup touch Tupfile ok.c ok
parse_fail_msg "Missing newline character"

cat > ok.c << HERE
#include <stdio.h>

int main(void)
{
	/* The six backslashes here becomes 3 in the C program, 2 of which
	 * become a backslash in the Tupfile, and 1 of which is used with
	 * the newline.
	 */
	printf(": foreach *.c |> \\\\\\ngcc -c %%f -o %%o |> %%B.o");
	return 0;
}
HERE
gcc ok.c -o ok.exe
./ok.exe > Tupfile

tup touch Tupfile
parse_fail_msg "Missing newline character"

eotup
