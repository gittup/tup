#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2018  Mike Shal <marfey@gmail.com>
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

# Make sure we can handle a Tupfile with a byte-order-mark set.

. ./tup.sh
cat > bom.c << HERE
#include <stdio.h>
int main(void)
{
	printf("%c%c%c", 0xef, 0xbb, 0xbf);
	return 0;
}
HERE
gcc bom.c -o bom.exe
(./bom.exe; echo ": foo |> cat %f > %o |> bar") > Tupfile
echo "some text" > foo
update

echo "some text" | diff - bar

rm Tupfile
(./bom.exe; echo "tup.rule('foo', 'cat %f > %o', 'bar')") > Tupfile.lua
update

echo "some text" | diff - bar

eotup
