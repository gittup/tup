#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2021  Mike Shal <marfey@gmail.com>
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

# Like t5013, but now with execv (at least gcc uses execv).

. ./tup.sh
cat > Tupfile << HERE
: foreach exec_test.c prog.c |> gcc %f -o %o |> %B.exe
: exec_test.exe prog.exe |> ./exec_test.exe && touch %o |> test_passed
HERE
const=""
if [ "$in_windows" = "1" ]; then
	const="const"
fi
cat > exec_test.c << HERE
#include <stdio.h>
#include <unistd.h>

int main(void)
{
	$const char * const args[] = {"prog.exe", NULL};
	execv("./prog.exe", args);
	return 1;
}
HERE
cat > prog.c << HERE
int main(void) {return 0;}
HERE
update

check_updates prog.c test_passed

eotup
