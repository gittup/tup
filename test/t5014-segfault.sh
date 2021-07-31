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

# Make sure a command that segfaults actually fails (and doesn't hose the DAG).

. ./tup.sh
check_no_windows segfault
check_no_osx fuse
cat > Tupfile << HERE
: segfault.c |> gcc %f -o %o |> tup_t5014_segfault
: tup_t5014_segfault |> ./%f |>
HERE

cat > segfault.c << HERE
int main(void)
{
	int *x = 0;
	*x = 5;
	return 0;
}
HERE
update_fail_msg "Segmentation fault"
tup_dep_exist . segfault.c . 'gcc segfault.c -o tup_t5014_segfault'
tup_dep_exist . 'gcc segfault.c -o tup_t5014_segfault' . tup_t5014_segfault

# Make sure the command runs and fails again.
update_fail_msg "Segmentation fault"

eotup
