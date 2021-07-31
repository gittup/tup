#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2021  Mike Shal <marfey@gmail.com>
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

# Try statfs

. ./tup.sh
check_no_windows statfs
check_no_ldpreload mozilla-unneeded

cat > ok.c << HERE
#include <stdio.h>
#ifdef __linux__
#include <sys/vfs.h>
#elif __APPLE__  || defined(__FreeBSD__)
#include <sys/mount.h>
#else
#error Please add support for this test in t4040-statfs.sh
#endif
int main(void)
{
	struct statfs buf;
	if(statfs("foo.txt", &buf) < 0) {
		perror("statfs");
		return 1;
	}
	return 0;
}
HERE

cat > Tupfile << HERE
: ok.c |> gcc %f -o %o |> ok
: ok |> ./%f |>
HERE
touch ok.c foo.txt Tupfile
update

tup_dep_exist . foo.txt . ./ok

eotup
