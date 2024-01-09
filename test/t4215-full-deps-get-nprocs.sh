#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2020-2024  Mike Shal <marfey@gmail.com>
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

# Make sure full-deps works with get_nprocs().
. ./tup.sh
check_no_osx sysinfo.h
check_no_windows get_nprocs
check_tup_suid

set_full_deps

cat > foo.c << HERE
#include <stdio.h>
#include <sys/sysinfo.h>

int main(void)
{
	int x = get_nprocs();
	if(x < 1) {
		printf("Error: %i nprocs\\n", x);
		return 1;
	}
	return 0;
}
HERE
cat > Tupfile << HERE
: foo.c |> gcc %f -o %o |> %B
: foo |> ./%f |>
HERE
update

tup_object_no_exist /sys/devices/system/cpu online

eotup
