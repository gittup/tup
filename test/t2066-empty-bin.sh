#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2013  Mike Shal <marfey@gmail.com>
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

# See if we can issue a command if a bin is empty.

. ./tup.sh
cat > Tupfile << HERE
obj-@(FOO) += foo.c
: foreach \$(obj-y) |> gcc -c %f -o %o |> %B.o {objs}

!ld = |> gcc -Wl,-r %f -o %o |>
!ld.EMPTY = |> ar crs %o |>
: {objs} |> !ld |> built-in.o
HERE
tup touch foo.c Tupfile
varsetall FOO=y
tup parse
tup_object_exist . 'gcc -c foo.c -o foo.o'
tup_dep_exist . 'foo.o' . 'gcc -Wl,-r foo.o -o built-in.o'
tup_object_no_exist . 'ar crs built-in.o'

varsetall FOO=n
tup parse
tup_object_no_exist . 'gcc -c foo.c -o foo.o'
tup_object_no_exist . 'gcc -Wl,-r foo.o -o built-in.o'
tup_object_exist . 'ar crs built-in.o'

eotup
