#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2015  Mike Shal <marfey@gmail.com>
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

# Put a group in a different path.

. ./tup.sh
tmkdir sub
tmkdir sub2
tmkdir sub3
cat > sub/Tupfile << HERE
: foreach *.h.in |> cp %f %o |> %B ../<foo-autoh>
HERE
cat > sub2/Tupfile << HERE
: foreach *.h.in |> cp %f %o |> %B ../<foo-autoh>
HERE
cat > sub3/Tupfile << HERE
: foreach *.c | ../<foo-autoh> |> gcc -c %f -o %o -I../sub -I../sub2 |> %B.o {objs}
HERE
echo '#define FOO 3' > sub/foo.h.in
echo '#define BAR 4' > sub2/bar.h.in
cat > sub3/foo.c << HERE
#include "foo.h"
#include "bar.h"
HERE
echo '#include "bar.h"' > sub3/bar.c
update

tup_dep_exist sub foo.h sub3 'gcc -c foo.c -o foo.o -I../sub -I../sub2'
tup_dep_exist sub2 bar.h sub3 'gcc -c foo.c -o foo.o -I../sub -I../sub2'
tup_dep_exist sub2 bar.h sub3 'gcc -c bar.c -o bar.o -I../sub -I../sub2'

eotup
