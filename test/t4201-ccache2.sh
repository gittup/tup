#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2024  Mike Shal <marfey@gmail.com>
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

# Try ccache and make sure that we track header dependencies correctly. If
# ccache is used in "direct mode", then it only parses the .c file and skips
# preprocessing, which means we miss all the header dependencies. Tup needs to
# force CCACHE_NODIRECT in order to ensure that things rebuild properly. Note
# that ccache itself doesn't track any sort of ghost dependency, so we have to
# use our own tracking to make sure we can rebuild successfully if a header is
# placed earlier in the include path.

. ./tup.sh

# Don't kill the user's ccache
export CCACHE_DIR=$PWD/.ccache

cat > Tupfile << HERE
export CCACHE_DIR
: foreach foo.c |> ccache gcc -Ia -Ib -c %f -o %o |> %B.o
: foo.o |> gcc %f -o %o |> foo.exe
HERE

mkdir a
mkdir b

echo '#define FOO 3' > b/foo.h

cat > foo.c << HERE
#include "foo.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv)
{
	if(atoi(argv[1]) != FOO) {
		printf("Mismatch: %i != %i\n", atoi(argv[1]), FOO);
		return 1;
	}
	return 0;
}
HERE
update

./foo.exe 3

# Need to sleep 1 otherwise the cache doesn't kick in
sleep 1

# Now rebuild foo.c so we use the ccache
touch foo.c
update

./foo.exe 3

echo '#define FOO 4' > a/foo.h
update
./foo.exe 4

eotup
