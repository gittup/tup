#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2023  Mike Shal <marfey@gmail.com>
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

# Try to use a %-flag inside the order-only prereqs in a !-macro.

. ./tup.sh
cat > Tupfile << HERE
!cc = foreach | %B.h |> gcc -c %f -o %o |> %B.o

: |> echo '#define FOO 3' > %o |> foo.h
: foo.c |> !cc |>
HERE
touch foo.c
update

tup_sticky_exist . foo.h . 'gcc -c foo.c -o foo.o'

eotup
