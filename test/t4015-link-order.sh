#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2023  Mike Shal <marfey@gmail.com>
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

# If we specify multiple inputs, make sure they come in the same order in the
# %f list. We check this by making an archive, which has to come after the
# object file in the command line.

. ./tup.sh

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: lib.o |> ar cr %o %f |> lib.a
: foo.o lib.a |> gcc %f -o %o |> prog.exe
HERE

echo "int foo(void) {return 3;}" > lib.c
cat > foo.c << HERE
int foo(void);
int main(void) {return foo();}
HERE
update

eotup
