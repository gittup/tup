#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2008-2021  Mike Shal <marfey@gmail.com>
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

mkdir bar
cp ../testTupfile.tup bar/Tupfile
echo "#define FOO 3" > foo.h
(echo "#include \"../foo.h\""; echo "int main(void) {return FOO;}") > bar/foo.c
touch foo.h
touch bar/foo.c
update
sym_check bar/foo.o main
tup_dep_exist . foo.h bar "gcc -c foo.c -o foo.o"
tup_dep_exist bar "gcc -c foo.c -o foo.o" bar foo.o

eotup
