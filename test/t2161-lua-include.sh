#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2021  Mike Shal <marfey@gmail.com>
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

# Include a lua script from a regular Tupfile. Variables should be preserved.

. ./tup.sh
cat > Tupfile << HERE
CFLAGS += -DFOO
CFLAGS += -DBAR
CFLAGS += -DBAZ
CC = gcc
include build.lua
HERE
cat > build.lua << HERE
files += 'foo.c'
tup.foreach_rule(files, '\$(CC) \$(CFLAGS) -c %f -o %o', '%B.o')
HERE
tup touch foo.c
update

tup_object_exist . 'gcc -DFOO -DBAR -DBAZ -c foo.c -o foo.o'

eotup
