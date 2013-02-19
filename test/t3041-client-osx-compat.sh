#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2013  Mike Shal <marfey@gmail.com>
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

# OSX tries to call access() on the @tup@ directory before calling getattr()
# on the @tup@/FOO variable. This test makes sure that access() on @tup@
# will succeed to support that.

. ./tup.sh
check_no_windows OSX-specific

cat > ok.c << HERE
#include <stdio.h>
#include <unistd.h>

int main(void)
{
	if(access("@tup@", R_OK) < 0) {
		perror("access @tup@");
		return 1;
	}
	return 0;
}
HERE
cat > Tupfile << HERE
: ok.c |> gcc %f -o %o |> ok
: ok |> ./ok |>
HERE
tup touch ok.c Tupfile
update

eotup
