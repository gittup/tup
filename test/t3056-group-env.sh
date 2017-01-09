#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2017  Mike Shal <marfey@gmail.com>
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

# Update PATH while we have a group.

. ./tup.sh
cat > Tupfile << HERE
: foreach *.h.in |> cp %f %o |> %B <foo-autoh>
: foreach *.c | <foo-autoh> |> gcc -c %f -o %o |> %B.o {objs}
HERE
echo '#define FOO 3' > foo.h.in
echo '#define BAR 4' > bar.h.in
cat > foo.c << HERE
#include "foo.h"
#include "bar.h"
HERE
echo '#include "bar.h"' > bar.c
update

tup_dep_exist . foo.h . 'gcc -c foo.c -o foo.o'
tup_dep_exist . bar.h . 'gcc -c foo.c -o foo.o'
tup_dep_exist . bar.h . 'gcc -c bar.c -o bar.o'

export PATH=$PATH:$PWD
update

eotup
