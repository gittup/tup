#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2016  Mike Shal <marfey@gmail.com>
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

# Make sure we can use gcc --coverage

. ./tup.sh

check_tup_suid

CC="gcc"
if [ "$tupos" = "Darwin" ]; then
	if ! which gcc-4.2 > /dev/null; then
		echo "Skipping test - OSX needs gcc-4.2 for code coverage" 1>&2
		eotup
	fi
	CC="gcc-4.2"
fi

cat > Tupfile << HERE
: |> ^c^ $CC --coverage foo.c -o %o |> foo.exe | foo.gcno
: |> ^c CC bar^ $CC --coverage bar.c -o %o |> bar.exe | bar.gcno
HERE

cat > foo.c << HERE
#include <stdio.h>
int main(int argc, char **argv)
{
	if(argc > 1) {
		return 0;
	} else {
		return 1;
	}
}
HERE

cp foo.c bar.c
tup touch foo.c bar.c Tupfile
update

if strings foo.exe | grep '\.tup/mnt' > /dev/null; then
	echo "Error: 'foo' executable shouldn't reference .tup/mnt directory" 1>&2
	exit 1
fi

./foo.exe abcd
check_exist foo.gcda

eotup
