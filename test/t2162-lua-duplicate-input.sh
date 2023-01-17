#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2023  Mike Shal <marfey@gmail.com>
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

# Duplicate inputs should be pruned, in lua.

. ./tup.sh
cat > Tupfile.lua << HERE
objs = tup.foreach_rule('*.c', 'gcc -c %f -o %o', '%B.o')
objs += 'foo.o'
tup.rule(objs, 'gcc %f -o %o', 'prog.exe')
HERE
echo 'int main(void) {return 0;}' > foo.c
update

tup_object_exist . 'gcc foo.o -o prog.exe'

eotup
