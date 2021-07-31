#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2021  Mike Shal <marfey@gmail.com>
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

# More duplicate input tests.

. ./tup.sh
cat > Tupfile << HERE
: foreach foo.c foo.c | foo.c foo.h foo.h |> gcc -c %f -o %o |> %B.o {objs}
: {objs} foo.o |> gcc %f -o %o |> prog.exe
: foo.c bar.c foo.c |> echo blah1 %f |>
: bar.c foo.c bar.c |> echo blah2 %f |>
HERE
(echo '#include "foo.h"'; echo 'int main(void) {return 0;}') > foo.c
touch foo.h bar.c
update

tup_object_exist . 'gcc -c foo.c -o foo.o'
tup_object_exist . 'gcc foo.o -o prog.exe'
tup_dep_exist . foo.c . 'gcc -c foo.c -o foo.o'
tup_dep_exist . foo.h . 'gcc -c foo.c -o foo.o'
tup_object_exist . 'echo blah1 foo.c bar.c'
tup_object_exist . 'echo blah2 bar.c foo.c'

eotup
