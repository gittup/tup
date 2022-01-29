#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2022  Mike Shal <marfey@gmail.com>
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

# This tries to do an fopen inside a constructor of a shared library linked
# into the main executable. Apparently this is what libselinux.so.1 does on
# the Ubuntu machine I'm using to test 64-bit, and it's linked into a bunch
# of utilities like cp and mv. This breaks because apparently my constructor
# in ldpreload isn't called until after all shared libraries are loaded.

. ./tup.sh
check_no_windows shlib
cat > Tupfile << HERE
: lib.c |> gcc -fPIC -shared %f -o %o |> lib.so
: prog.c lib.so |> gcc %f -o %o |> prog
: prog |> LD_LIBRARY_PATH=. ./%f |>
HERE
cat > lib.c << HERE
#include <stdio.h>
/* Constructor! The burninator! */
static void my_init(void) __attribute__((constructor));
static void my_init(void) {fopen("foo.txt", "r");}
HERE
cat > prog.c << HERE
int main(void) {return 0;}
HERE
update

eotup
