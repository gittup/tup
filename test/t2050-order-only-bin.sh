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

# Try order-only-ish prerequisites. Now with bins! (copied from t2011)

. ./tup.sh
cat > Tupfile << HERE
: |> echo blah > %o |> foo.h {headers}
: foreach *.c | {headers} |> echo gcc -c %f -o %o |> %B.o
HERE

tup touch Tupfile foo.c bar.c
parse
tup_sticky_exist . foo.h . "echo gcc -c foo.c -o foo.o"
tup_sticky_exist . foo.h . "echo gcc -c bar.c -o bar.o"

eotup
