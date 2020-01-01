#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2020  Mike Shal <marfey@gmail.com>
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

# This was an error case I discovered when I tried to move sysvinit out of
# gittup and compile everything. For some reason a directory gets marked as
# both create and delete, but the updater still tries to open the directory and
# update it. Basically we just set up some directories (so things are in
# create), then move it to another directory (so it goes in delete).

. ./tup.sh
check_monitor_supported
monitor
mkdir a
mkdir a/a2
cp ../testTupfile.tup a/a2/Tupfile

echo "int main(void) {return 0;}" > a/a2/foo.c
tup flush
mv a b
update
tup_object_exist . b
tup_object_exist b a2
tup_object_exist b/a2 foo.c foo.o prog.exe 'gcc -c foo.c -o foo.o' 'gcc foo.o -o prog.exe'
tup_object_no_exist . a
tup_object_no_exist a a2
tup_object_no_exist a/a2 foo.c foo.o prog.exe 'gcc -c foo.c -o foo.o' 'gcc foo.o -o prog.exe'

eotup
