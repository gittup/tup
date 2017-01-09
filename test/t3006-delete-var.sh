#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2017  Mike Shal <marfey@gmail.com>
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

# Verify that removing a variable from the database will cause a dependent
# Tupfile to be re-parsed.

. ./tup.sh
cat > Tupfile << HERE
src-y:=
src-@(BLAH) = foo.c
: foreach \$(src-y) |> gcc -c %f -o %o |> %B.o
HERE
tup touch Tupfile foo.c
varsetall BLAH=y
update
tup_object_exist . "gcc -c foo.c -o foo.o"
check_exist foo.o

varsetall

# BLAH isn't set, so it is treated as empty
update
tup_object_no_exist . "gcc -c foo.c -o foo.o"
check_not_exist foo.o

eotup
