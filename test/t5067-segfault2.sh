#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2021  Mike Shal <marfey@gmail.com>
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

# Make sure that if a command creates a file and then segfaults, the outputs
# the outputs are still checked.
. ./tup.sh
check_no_windows segfault
check_no_osx fuse

cat > ok.c << HERE
#include <stdio.h>

int main(void)
{
	FILE *f;
	int *x = 0;

	f = fopen("tmp.txt", "w");
	if(!f) {
		fprintf(stderr, "Unable to open tmp.txt file for write in t5067.\n");
		return 1;
	}
	*x = 5;
	return 0;
}
HERE
cat > Tupfile << HERE
: ok.c |> gcc %f -o %o |> tup_t5067_segfault2
: tup_t5067_segfault2 |> ./%f |> tmp.txt
HERE
update_fail_msg "Segmentation fault"

cat > Tupfile << HERE
HERE
update
check_not_exist tmp.txt tup_t5067_segfault2

eotup
