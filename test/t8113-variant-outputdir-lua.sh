#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2023-2024  Mike Shal <marfey@gmail.com>
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

# Same as t8108 (test for TUP_VARIANT_OUTPUTDIR), but with Lua
. ./tup.sh

mkdir build

cat > Tuprules.lua << HERE
tup.include('rules/stuff.lua')
HERE

mkdir rules
cat > rules/stuff.lua << HERE
CFLAGS += "getcwd=" .. tup.getcwd()
CFLAGS += "getvariantdir=" .. tup.getvariantdir()
CFLAGS += "getvariantoutputdir=" .. tup.getvariantoutputdir()
HERE

mkdir sub
mkdir sub/dir
cat > sub/dir/Tupfile.lua << HERE
tup.rule('echo ' .. CFLAGS .. ' > %o', {'out.txt'})
HERE
update

gitignore_good 'getcwd=../../rules' sub/dir/out.txt
gitignore_good 'getvariantdir=../../rules' sub/dir/out.txt
gitignore_good 'getvariantoutputdir=\.' sub/dir/out.txt

touch build/tup.config
update

gitignore_good 'getcwd=../../rules' build/sub/dir/out.txt
gitignore_good 'getvariantdir=../../build/rules' build/sub/dir/out.txt
gitignore_good 'getvariantoutputdir=../../build/sub/dir' build/sub/dir/out.txt

eotup
