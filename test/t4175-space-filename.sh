#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2015-2023  Mike Shal <marfey@gmail.com>
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

. ./tup.sh
check_no_windows single-quotes

cat > Tupfile << HERE
: foreach *.c |> gcc -c %"f -o %"o |> %B.o
: *.o |> gcc %'f -o %'o |> prog.exe
HERE
echo "int main(void) {return 0;}" > foo.c
echo "int spacetest;" > space\ bar.c
update

sym_check prog.exe spacetest

tup_object_exist . 'gcc -c "space bar.c" -o "space bar.o"'
tup_object_exist . 'gcc -c "foo.c" -o "foo.o"'
tup_object_exist . "gcc 'foo.o' 'space bar.o' -o 'prog.exe'"

eotup
