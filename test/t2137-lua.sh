#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2014  Mike Shal <marfey@gmail.com>
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

# Check a basic rule in a Tupfile.lua

. ./tup.sh
cat > Tupfile.lua << HERE
tup.rule('echo hey')
HERE
tup touch Tupfile.lua
update
tup_object_exist . "echo hey"

cat > Tupfile.lua << HERE
tup.rule({'foo.c'}, 'echo hey')
HERE
tup touch foo.c Tupfile.lua
update
tup_object_exist . "echo hey"

cat > Tupfile.lua << HERE
tup.rule('touch output', {'output'})
HERE
tup touch foo.c Tupfile.lua
update
tup_object_exist . "touch output"
check_exist output

cat > Tupfile.lua << HERE
tup.rule('foo.c', 'touch output', 'output')
HERE
tup touch Tupfile.lua
update
tup_object_exist . "touch output"
check_exist output

eotup
