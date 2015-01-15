#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2015  Mike Shal <marfey@gmail.com>
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

# This test checks for the issue where foreach on a different directory
# produced commands attached to the wrong dir (specifically, in the
# subdirectory rather than the parent directory).
. ./tup.sh

cat > Tupfile << HERE
: foreach input/*.o |> cat %f > %o |> %b
HERE

tmkdir input
cat > input/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE

echo "void foo(void) {}" > input/foo.c
echo "void bar(void) {}" > input/bar.c

tup touch Tupfile input/Tupfile input/foo.c input/bar.c
update

eotup
