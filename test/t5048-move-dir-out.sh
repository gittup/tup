#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2017  Mike Shal <marfey@gmail.com>
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

# Try to move a directory out of tup, and make sure dependent files are still
# compiled.

. ./tup.sh
mkdir real
cd real
re_init
tmkdir sub
echo "#define FOO 3" > sub/foo.h
echo '#include "foo.h"' > foo.c
echo ': foreach *.c |> gcc -c %f -o %o -Isub |> %B.o' > Tupfile
tup touch Tupfile foo.c sub/foo.h
update

mv sub ..
tup rm sub
update_fail

tup touch foo.h
update

eotup
