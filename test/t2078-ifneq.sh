#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2015  Mike Shal <marfey@gmail.com>
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

# Test ifneq

. ./tup.sh
cat > Tupfile << HERE
ifneq (\$(CONFIG_FOO),22)
objs-y += foo.c
endif

ifneq (\$(CONFIG_FOO),23)
objs-y += bar.c
endif

: foreach \$(objs-y) |> gcc -c %f -o %o |> %B.o
HERE
tup touch foo.c bar.c Tupfile
tup parse
tup_object_exist . 'gcc -c foo.c -o foo.o'
tup_object_exist . 'gcc -c bar.c -o bar.o'

varsetall FOO=y
tup parse
tup_object_exist . 'gcc -c foo.c -o foo.o'
tup_object_exist . 'gcc -c bar.c -o bar.o'

varsetall
tup parse
tup_object_exist . 'gcc -c foo.c -o foo.o'
tup_object_exist . 'gcc -c bar.c -o bar.o'

varsetall FOO=22
tup parse
tup_object_no_exist . 'gcc -c foo.c -o foo.o'
tup_object_exist . 'gcc -c bar.c -o bar.o'

varsetall FOO=23
tup parse
tup_object_exist . 'gcc -c foo.c -o foo.o'
tup_object_no_exist . 'gcc -c bar.c -o bar.o'

cat > Tupfile << HERE
: foreach \$(objs-y) |> gcc -c %f -o %o |> %B.o
HERE
tup touch Tupfile
tup parse
tup_object_no_exist . 'gcc -c foo.c -o foo.o'
tup_object_no_exist . 'gcc -c bar.c -o bar.o'

eotup
