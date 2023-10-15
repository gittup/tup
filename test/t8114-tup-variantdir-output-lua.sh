#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2023  Mike Shal <marfey@gmail.com>
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

# Same as t8109 but in lua.
. ./tup.sh

cat > Tuprules.lua << HERE
LIBS_DIR = tup.getvariantdir() .. '/libs'
HERE

mkdir lib
cat > lib/Tupfile.lua << HERE
tup.foreach_rule('*.c', 'gcc -c %f -o %o', '%B.o')
tup.rule('*.o', 'ar crs %o %f', {LIBS_DIR .. '/libfoo.a', extra_outputs={LIBS_DIR .. '/<libs>'}})
HERE

mkdir sub
cat > sub/Tupfile.lua << HERE
tup.foreach_rule('*.c', 'gcc -c %f -o %o', '%B.o')
tup.rule({'*.o', extra_inputs={LIBS_DIR .. '/<libs>'}}, 'gcc %f -L' .. LIBS_DIR .. ' -lfoo -o %o', 'main.exe')
HERE

echo 'int foo(void) {return 7;}' > lib/foo.c
cat > sub/main.c << HERE
int foo(void);
int main(void) {return foo();}
HERE
update

mkdir build
touch build/tup.config
update

eotup
