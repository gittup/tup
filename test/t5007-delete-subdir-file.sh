#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2008-2024  Mike Shal <marfey@gmail.com>
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
mkdir blah
cp ../testTupfile.tup blah/Tupfile

# Verify all files are compiled
echo "int main(void) {return 0;}" > blah/foo.c
echo "void bar1(void) {}" > blah/bar.c
echo "void baz1(void) {}" > blah/baz.c
update
sym_check blah/foo.o main
sym_check blah/bar.o bar1
sym_check blah/baz.o baz1
sym_check blah/prog.exe main bar1 baz1

# When baz.c is deleted, baz.o should be deleted as well, and prog.exe should be
# re-linked. The baz.[co] objects should be removed from .tup
rm blah/baz.c
update
check_not_exist blah/baz.o
sym_check blah/prog.exe main bar1 ^baz1

tup_object_exist blah foo.c foo.o bar.c bar.o prog.exe
tup_object_no_exist blah baz.c baz.o

rm blah/foo.c blah/bar.c
update
check_not_exist blah/foo.o blah/bar.o blah/prog.exe
tup_object_no_exist blah foo.c foo.o bar.c bar.o prog.exe

eotup
