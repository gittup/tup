#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2008-2018  Mike Shal <marfey@gmail.com>
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

# Same as the previous test, only we try some variable assignments.

. ./tup.sh
cat > Tupfile << HERE
CC = gcc
CCARGS := -DFOO=1
CCARGS += -DBAR=1
CC = echo \$(CC)
: foreach *.c |> \$(CC) -c %f -o %o \$(CCARGS) |> %B.o
: *.o |> \$(CC) -o prog %f |> prog
HERE
tup touch foo.c bar.c Tupfile
parse
tup_object_exist . foo.c bar.c
tup_object_exist . "echo gcc -c foo.c -o foo.o -DFOO=1 -DBAR=1"
tup_object_exist . "echo gcc -c bar.c -o bar.o -DFOO=1 -DBAR=1"
tup_object_exist . "echo gcc -o prog bar.o foo.o"

eotup
