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

echo "int main(void) {} void foo(void) {}" > foo.c
update
sym_check foo.o foo
sym_check prog.exe foo

cat Tupfile | sed 's/prog/newprog/g' > tmpTupfile
mv tmpTupfile Tupfile
update

sym_check newprog.exe foo
check_not_exist prog.exe
tup_object_no_exist . "gcc foo.o -o prog.exe"
tup_object_no_exist . prog.exe

eotup
