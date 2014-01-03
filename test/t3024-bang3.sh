#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2014  Mike Shal <marfey@gmail.com>
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

# See if we can set the 'foreach' flag in the !-macro itself

. ./tup.sh
cat > Tupfile << HERE
!cc = foreach |> gcc -c %f -o %o |>

files += foo.c
files += bar.c
: \$(files) |> !cc |> %B.o
HERE
tup touch foo.c bar.c Tupfile
update

check_exist foo.o bar.o
tup_dep_exist . foo.c . 'gcc -c foo.c -o foo.o'
tup_dep_exist . bar.c . 'gcc -c bar.c -o bar.o'

eotup
