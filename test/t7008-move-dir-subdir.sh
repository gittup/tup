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

# Check if we move a dir that depends on its subdir, that things work.

. ./tup.sh
check_monitor_supported
monitor
mkdir a
mkdir a/a2

cat > a/Tupfile << HERE
: foreach a2/*.c |> gcc -c %f -o %o |> %B.o
HERE
echo "int main(void) {return 0;}" > a/a2/foo.c
update

tup_object_exist . a
tup_object_exist a a2 foo.o 'gcc -c a2/foo.c -o foo.o'
tup_object_exist a/a2 foo.c
sym_check a/foo.o main

mv a b
update
tup_object_no_exist . a
tup_object_no_exist a a2 foo.o 'gcc -c a2/foo.c -o foo.o'
tup_object_no_exist a/a2 foo.c

tup_object_exist . b
tup_object_exist b a2 foo.o 'gcc -c a2/foo.c -o foo.o'
tup_object_exist b/a2 foo.c
sym_check b/foo.o main

eotup
