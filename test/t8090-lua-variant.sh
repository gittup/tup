#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2018  Mike Shal <marfey@gmail.com>
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

# Try a variant in lua.
. ./tup.sh
check_no_windows variant

tmkdir build
tmkdir sub

cat > Tupfile.lua << HERE
objs = tup.foreach_rule('*.c', 'gcc -c %f -o %o', '%B.o')
objs += 'sub/*.o'
tup.rule(objs, 'gcc %f -o %o', 'prog.exe')
HERE
cat > sub/Tupfile.lua << HERE
tup.foreach_rule('*.c', 'gcc -c %f -o %o', '%B.o')
HERE
echo "int main(void) {return 0;}" > foo.c
echo "CONFIG_FOO=y" > build/tup.config
tup touch build/tup.config Tupfile.lua foo.c sub/bar.c
update

check_exist build/foo.o build/sub/bar.o build/prog.exe
check_not_exist foo.o sub/bar.o prog.exe

tup touch baz.c
update
check_exist build/baz.o

eotup
