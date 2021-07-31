#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2008-2021  Mike Shal <marfey@gmail.com>
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

# Now try to include a Tupfile using a relative path. From there, also include
# another file relative to it (Tupfile.vars and Tupfile.ccargs are in the same
# directory).

. ./tup.sh
mkdir a
cat > a/Tupfile << HERE
include ../Tupfile.vars
: foreach *.c |> \$(CC) -c %f -o %o \$(CCARGS) |> %B.o
: *.o |> \$(CC) -o prog %f |> prog
HERE

cat > Tupfile.vars << HERE
CC = gcc
include Tupfile.ccargs
HERE

cat > Tupfile.ccargs << HERE
CCARGS := -DFOO=1
CCARGS += -DBAR=1
HERE

touch a/foo.c a/bar.c
parse
tup_object_exist a foo.c bar.c
tup_object_exist a "gcc -c foo.c -o foo.o -DFOO=1 -DBAR=1"
tup_object_exist a "gcc -c bar.c -o bar.o -DFOO=1 -DBAR=1"
tup_object_exist a "gcc -o prog bar.o foo.o"

eotup
