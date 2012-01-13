#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2012  Mike Shal <marfey@gmail.com>
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
tmkdir a
cp ../testTupfile.tup a/Tupfile

echo "int main(void) {return 0;}" > a/foo.c
tup touch a/foo.c a/Tupfile
update
tup_object_exist . a
tup_object_exist a foo.c foo.o prog.exe
sym_check a/foo.o main
sym_check a/prog.exe main

# Move directory a to b
mv a b
tup rm a
tup touch b b/foo.c b/Tupfile
# TODO: instead of --no-scan, use --overwrite-outputs or some such
update --no-scan
tup_object_exist . b
tup_object_exist b foo.c foo.o prog.exe
tup_object_no_exist . a
tup_object_no_exist a foo.c foo.o prog.exe

eotup
