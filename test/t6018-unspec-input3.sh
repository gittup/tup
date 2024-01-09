#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2024  Mike Shal <marfey@gmail.com>
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

# Specify a header as an auto dependency, but the C file doesn't actually
# include it. The update will cause the link to go away. Then if we change the
# C file to include the header (without changing the Tupfile), we get yelled at
# even though the dependency is specified in the Tupfile. This dependency
# should stick around somehow.
. ./tup.sh
cat > Tupfile << HERE
: foo.h.in |> cp %f %o |> %B
: foreach *.c | foo.h |> gcc -c %f -o %o |> %B.o
HERE

echo "#define FOO 3" > foo.h.in
cat > foo.c << HERE
#define FOO 4
int main(void) {return FOO;}
HERE
update

cat > foo.c << HERE
#include "foo.h"
int main(void) {return FOO;}
HERE
update

eotup
