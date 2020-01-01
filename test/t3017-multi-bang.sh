#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2020  Mike Shal <marfey@gmail.com>
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

# Test bang macros with multiple paths. This is useful in the kernel Tupfiles
# to order the objects correctly when using local .c files and built-in.o files
# in subdirectories.

. ./tup.sh
tmkdir sub
cat > Tupfile << HERE
!cc_linux.c = |> gcc -c %f -o %o |> %B.o
!cc_linux.o = |> |>
: foreach foo.c sub/built-in.o bar.c |> !cc_linux |> {objs}
: {objs} |> echo %f |>
HERE
tup touch foo.c bar.c sub/built-in.o Tupfile
update

check_exist foo.o bar.o
tup_object_exist . 'gcc -c foo.c -o foo.o'
tup_object_exist . 'gcc -c bar.c -o bar.o'
tup_object_no_exist . 'gcc -c sub/built-in.o -o built-in.o'
tup_object_exist . 'echo foo.o sub/built-in.o bar.o'

eotup
