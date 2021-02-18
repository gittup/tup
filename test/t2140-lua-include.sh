#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2021  Mike Shal <marfey@gmail.com>
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

# Now try to include the variables from another Tupfile.lua

. ./tup.sh
cat > Tupfile.lua << HERE
tup.include 'vars.lua'
tup.foreach_rule('*.c', '\$(CC) -c %f -o %o \$(CCARGS)', '%B.o')
tup.rule('*.o', '\$(CC) -o %o %f', 'prog.exe')
HERE

cat > vars.lua << HERE
CC = 'gcc'
CCARGS = '-DFOO=1'
CCARGS += '-DBAR=1'
HERE

echo "int main(void) {return 0;}" > foo.c
touch bar.c
tup touch foo.c bar.c Tupfile.lua vars.lua
update
tup_object_exist . foo.c bar.c
tup_object_exist . "gcc -c foo.c -o foo.o -DFOO=1 -DBAR=1"
tup_object_exist . "gcc -c bar.c -o bar.o -DFOO=1 -DBAR=1"
tup_object_exist . "gcc -o prog.exe bar.o foo.o"

# Now change the compiler to 'gcc -W' and verify that we re-parse the parent
# Tupfile.lua to generate new commands and get rid of the old ones.
cat > vars.lua << HERE
CC = 'gcc -W'
CCARGS = '-DFOO=1'
CCARGS += '-DBAR=1'
HERE
tup touch vars.lua
update
tup_object_no_exist . "gcc -c foo.c -o foo.o -DFOO=1 -DBAR=1"
tup_object_no_exist . "gcc -c bar.c -o bar.o -DFOO=1 -DBAR=1"
tup_object_no_exist . "gcc -o prog.exe bar.o foo.o"
tup_object_exist . "gcc -W -c foo.c -o foo.o -DFOO=1 -DBAR=1"
tup_object_exist . "gcc -W -c bar.c -o bar.o -DFOO=1 -DBAR=1"
tup_object_exist . "gcc -W -o prog.exe bar.o foo.o"

eotup
