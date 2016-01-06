#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2016  Mike Shal <marfey@gmail.com>
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

# The endif here doesn't get parsed and the ifeq statement ends up being false
# through the end. Tup should catch this and report it as an error.

. ./tup.sh
cat > Tupfile << HERE
!cc = |> gcc -c %f -o %o |>
ifeq (1,2)
: a.c |> !cc |> a.o
endif #foo == 2
: b.c |> !cc |> b.o
HERE
tup touch a.c b.c Tupfile
parse_fail_msg "missing endif before EOF"

cat > Tupfile << HERE
!cc = |> gcc -c %f -o %o |>
ifeq (1,2)
: a.c |> !cc |> a.o
endif
: b.c |> !cc |> b.o
HERE
tup touch Tupfile
parse
tup_object_exist . "gcc -c b.c -o b.o"

eotup
