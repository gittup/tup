#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2014  Mike Shal <marfey@gmail.com>
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

# Reference a group in a !-macro.

. ./tup.sh
tmkdir sub
tmkdir sub2
tmkdir sub2/blah
tmkdir sub3
cat > Tuprules.tup << HERE
PROJ_ROOT = \$(TUP_CWD)
CFLAGS += -I\$(TUP_CWD)
!headergen = |> cp %f %o |> %B \$(PROJ_ROOT)/<autoh>
!cc = | \$(PROJ_ROOT)/<autoh> |> gcc -c %f -o %o \$(CFLAGS) |>
HERE
cat > sub/Tupfile << HERE
include_rules
: foreach *.h.in |> !headergen |>
HERE
cat > sub2/blah/Tupfile << HERE
include_rules
: foreach *.h.in |> !headergen |>
HERE
cat > sub3/Tupfile << HERE
include_rules
: foreach *.c |> !cc |> %B.o {objs}
HERE
echo '#define FOO 3' > sub/foo.h.in
echo '#define BAR 4' > sub2/blah/bar.h.in
cat > sub3/foo.c << HERE
#include "sub/foo.h"
#include "sub2/blah/bar.h"
HERE
echo '#include "sub2/blah/bar.h"' > sub3/bar.c
update

tup_dep_exist sub foo.h sub3 'gcc -c foo.c -o foo.o -I..'
tup_dep_exist sub2/blah bar.h sub3 'gcc -c foo.c -o foo.o -I..'
tup_dep_exist sub2/blah bar.h sub3 'gcc -c bar.c -o bar.o -I..'

eotup
