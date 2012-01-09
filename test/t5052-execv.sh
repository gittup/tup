#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2012  Mike Shal <marfey@gmail.com>
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
: foreach exec_test.c exe.c |> gcc %f -o %o |> %B
: exec_test exe |> ./exec_test && touch %o |> test_passed
HERE
cat > exec_test.c << HERE
#include <unistd.h>

int main(void)
{
	char * const args[] = {"exe", NULL};
	execv("./exe", args);
	return 1;
}
HERE
cat > exe.c << HERE
int main(void) {return 0;}
HERE
tup touch Tupfile exec_test.c exe.c
update

check_updates exe.c test_passed

eotup
