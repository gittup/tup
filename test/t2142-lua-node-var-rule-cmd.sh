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

# Test using a node-variable in a rule command line.

. ./tup.sh

mkdir sw
mkdir sw/toolkit
mkdir sw/app

cat > sw/Tuprules.lua << HERE
toolkit_lib = tup.nodevariable('toolkit/toolkit.a')
HERE

cat > sw/app/Tupfile.lua << HERE
tup.definerule{command = 'cp ' .. toolkit_lib .. ' lib_copy.a', outputs = {'lib_copy.a'}}
HERE

touch sw/Tuprules.lua
touch sw/toolkit/toolkit.a
touch sw/app/Tupfile.lua
update

tup_dep_exist sw/toolkit toolkit.a sw/app "cp ../toolkit/toolkit.a lib_copy.a"

eotup
