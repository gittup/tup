#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2012  Mike Shal <marfey@gmail.com>
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

# Test bang macros.

. ./tup.sh
cat > Tupfile << HERE
headers = foo.h
!cc = | foo.h |> gcc -c %f -o %o |>
: foo.h.in |> $tupcp %f %o |> foo.h
: foreach *.c |> !cc |> %B.o
HERE
echo '#define FOO 3' > foo.h.in
echo '#include "foo.h"' > foo.c
tup touch foo.h.in foo.c bar.c Tupfile
update

check_exist foo.o bar.o
tup_dep_exist . foo.h . 'gcc -c foo.c -o foo.o'
tup_dep_exist . foo.h . 'gcc -c bar.c -o bar.o'

check_updates foo.h.in foo.o
check_no_updates foo.h.in bar.o

eotup
