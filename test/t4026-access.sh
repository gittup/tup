#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2023  Mike Shal <marfey@gmail.com>
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

# Try to use the access() function. It should always count as a read (similar
# to a stat()).

. ./tup.sh
check_no_ldpreload mozilla-unneeded

cat > foo.c << HERE
#include <unistd.h>

int main(void)
{
	if(access("access.txt", F_OK) != 0)
		return 1;
	return 0;
}
HERE
cat > Tupfile << HERE
: foo.c |> gcc %f -o %o |> foo.exe
: foo.exe |> ./%f |>
HERE
touch access.txt
update

tup_dep_exist . access.txt . "./foo.exe"

eotup
