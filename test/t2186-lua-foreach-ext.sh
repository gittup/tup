#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2024  Mike Shal <marfey@gmail.com>
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

# Try to trigger an issue I had with extra_inputs referencing something in
# vardb.

. ./tup.sh

cat > Tupfile.lua << HERE
CFLAGS_c = '-Dcfile'
CFLAGS_S = '-DSfile'
inputs = '*.c'
inputs += '*.S'
tup.foreach_rule(inputs, 'gcc -c %f \$(CFLAGS_%e) -o %o', '%B.o')
HERE
touch foo.c bar.S
parse

tup_object_exist . 'gcc -c foo.c -Dcfile -o foo.o'
tup_object_exist . 'gcc -c bar.S -DSfile -o bar.o'

eotup
