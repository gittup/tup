#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2008-2014  Mike Shal <marfey@gmail.com>
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
cp ../testTupfile.tup Tupfile

echo "int main(void) {return 0;}" > foo.c
echo "void bar1(void) {}" > bar.c
tup touch foo.c bar.c
update
sym_check foo.o main
sym_check bar.o bar1
sym_check prog.exe main bar1

# When bar.o is deleted, it should be re-generated
rm bar.o
tup rm bar.o
update
sym_check bar.o bar1

# Similar for prog.exe
rm prog.exe
tup rm prog.exe
update
sym_check prog.exe main bar1

eotup
