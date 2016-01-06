#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2016  Mike Shal <marfey@gmail.com>
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

# Try to include a header that was auto-generated from another command. It's
# possible based on the ordering of the commands that the header will be
# generated before the file is compiled, so things will appear to work.
# However, if the header isn't specified as a dependency for the compilation
# command then things could break during a parallel build or if things get
# re-ordered later. So, that should be detected and return a failure.
. ./tup.sh
cat > Tupfile << HERE
: foo.h.in |> cp %f %o |> %B
HERE

echo "#define FOO 3" > foo.h.in
tup touch foo.h.in Tupfile
update

cat > foo.c << HERE
#include "foo.h"
int main(void) {return FOO;}
HERE
cat > Tupfile << HERE
: foo.h.in |> cp %f %o |> %B
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
tup touch foo.c Tupfile
update_fail

eotup
