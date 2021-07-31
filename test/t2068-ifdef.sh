#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2021  Mike Shal <marfey@gmail.com>
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

# Test ifdef

. ./tup.sh
cat > Tupfile << HERE
ifdef FOO
objs-y += foo.c
endif
: foreach \$(objs-y) |> gcc -c %f -o %o |> %B.o
HERE
touch foo.c Tupfile
parse
tup_object_no_exist . 'gcc -c foo.c -o foo.o'
tup_dep_exist tup.config FOO 0 .

varsetall FOO=y
parse
tup_object_exist . 'gcc -c foo.c -o foo.o'
tup_dep_exist tup.config FOO 0 .

varsetall
parse
tup_object_no_exist . 'gcc -c foo.c -o foo.o'
tup_dep_exist tup.config FOO 0 .

varsetall FOO=n
parse
tup_object_exist . 'gcc -c foo.c -o foo.o'
tup_dep_exist tup.config FOO 0 .

cat > Tupfile << HERE
: foreach \$(objs-y) |> gcc -c %f -o %o |> %B.o
HERE
touch Tupfile
parse
tup_object_no_exist . 'gcc -c foo.c -o foo.o'
tup_dep_no_exist tup.config FOO 0 .

eotup
