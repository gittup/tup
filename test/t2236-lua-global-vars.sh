#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2022  Mike Shal <marfey@gmail.com>
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

# Make sure global variables in Tupfile.lua's don't cross-pollute.

. ./tup.sh

mkdir a
mkdir b
# a's Tupfile.lua sets a variable, then tries to use a file from b/ which will
# pause the parsing of a and switch to b.
cat > a/Tupfile.lua << HERE
var = 5
tup.rule('../b/file.txt', 'cp %f %o', 'out.txt')
tup.rule('echo \$(var)')
HERE
cat > b/Tupfile.lua << HERE
var = 17
tup.rule('touch %o', {'file.txt'})
tup.rule('echo \$(var)')
HERE

parse

tup_object_exist a 'echo 5'
tup_object_exist b 'echo 17'

eotup
