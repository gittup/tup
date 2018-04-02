#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018  Mike Shal <marfey@gmail.com>
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

# Make sure we can successfully get a dependency on a symlink that uses a full path.
. ./tup.sh
check_no_windows symlink
check_tup_suid

cwd=$PWD
cat > foo.c << HERE
#include "bar.h"
HERE
cat > Tupfile << HERE
: $cwd/foo.h |> !tup_ln |> bar.h <headers>
: foreach *.c | <headers> |> ^c^ gcc -c %f -o %o |> %B.o
HERE
tup touch Tupfile foo.h
update

tup_dep_exist . bar.h . 'gcc -c foo.c -o foo.o'
tup_dep_exist . foo.h . 'gcc -c foo.c -o foo.o'

eotup
