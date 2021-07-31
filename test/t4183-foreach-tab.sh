#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2016-2021  Mike Shal <marfey@gmail.com>
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

# Test that a Tupfile with a tab as a separator works
. ./tup.sh

cat > Tupfile << HERE
: foreach	file.c |> gcc -c %f -o %o |> %B.o
HERE
touch file.c
update

cat > Tupfile << HERE
!cc = foreach	| blah.h |> gcc -c %f -o %o |> %B.o
: foo.c |> !cc |>
HERE
touch foo.c blah.h
update

eotup
