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
cp ../testTupfile.tup Tupfile

# Verify all files are compiled
echo "int main(void) {return 0;}" > foo.c
echo "void bar1(void) {}" > bar.c
echo "void baz1(void) {}" > baz.c
update
sym_check foo.o main
sym_check bar.o bar1
sym_check baz.o baz1
sym_check prog.exe main bar1 baz1

# When baz.c is deleted, baz.o should be deleted as well, and prog.exe should be
# re-linked. The baz.[co] objects should be removed from .tup
rm baz.c
update
check_not_exist baz.o
sym_check prog.exe main bar1 ^baz1

tup_object_exist . foo.c foo.o bar.c bar.o prog.exe
tup_object_no_exist . baz.c baz.o

rm foo.c bar.c
update
check_not_exist foo.o bar.o prog.exe
tup_object_no_exist . foo.c foo.o bar.c bar.o prog.exe

eotup
