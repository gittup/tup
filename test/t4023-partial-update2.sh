#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2022  Mike Shal <marfey@gmail.com>
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

# Make sure 'tup upd foo.c; tup upd foo.o' will still update the object file.

. ./tup.sh
cp ../testTupfile.tup Tupfile

echo "int main(void) {}" > foo.c
update

echo "int foo; int main(void) {}" > foo.c
update_partial foo.c
update_partial foo.o

sym_check foo.o foo
sym_check prog.exe ^foo

update
sym_check prog.exe foo

eotup
