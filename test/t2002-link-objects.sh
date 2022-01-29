#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2008-2022  Mike Shal <marfey@gmail.com>
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

# Since *.o isn't 'touched', we have to get them from the output of the
# first rule.

. ./tup.sh
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> gcc -o prog %f |> prog
HERE
touch foo.c bar.c
parse
tup_object_exist . foo.c bar.c
tup_object_exist . "gcc -c foo.c -o foo.o"
tup_object_exist . "gcc -c bar.c -o bar.o"
tup_object_exist . "gcc -o prog bar.o foo.o"

eotup
