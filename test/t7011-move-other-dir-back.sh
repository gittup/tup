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

# Kinda like t7009, only we don't mess with the directory that has the Tupfile.

. ./tup.sh
check_monitor_supported
monitor
mkdir a
mkdir output

cat > output/Tupfile << HERE
: foreach ../a/*.c |> gcc -c %f -o %o |> %B.o
HERE
echo "int main(void) {return 0;}" > a/foo.c
update
tup_object_exist . a
tup_object_exist a foo.c
tup_object_exist output foo.o 'gcc -c ../a/foo.c -o foo.o'
sym_check output/foo.o main

# There and back again.
mv a b
tup flush
mv b a
update
tup_object_exist . a
tup_object_exist a foo.c
tup_object_exist output foo.o 'gcc -c ../a/foo.c -o foo.o'
sym_check output/foo.o main

tup_object_no_exist . b
tup_object_no_exist b foo.c

eotup
