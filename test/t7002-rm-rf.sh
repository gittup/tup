#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2008-2020  Mike Shal <marfey@gmail.com>
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
check_monitor_supported
monitor
mkdir a
mkdir a/a2
cp ../testTupfile.tup a/a2/Tupfile

echo "int main(void) {return 0;}" > a/a2/foo.c
update
tup_object_exist . a
tup_object_exist a a2
tup_object_exist a/a2 foo.c foo.o prog.exe
sym_check a/a2/foo.o main
sym_check a/a2/prog.exe main

rm -rf a
update
tup_object_no_exist . a
tup_object_no_exist a a2
tup_object_no_exist a/a2 foo.c foo.o prog.exe

eotup
