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

# Read from a data file with lua in a subdir.

. ./tup.sh
mkdir sub
cat > sub/data.txt << HERE
foo
bar
HERE
cat > sub/Tupfile.lua << HERE
local f = assert(io.open('data.txt', 'r'))
local outputs = {}
for line in f:lines() do
	outputs += line
end
f:close()
tup.rule({}, 'touch %o', outputs)
HERE
update

check_exist sub/foo sub/bar
tup_object_exist sub 'touch foo bar'

cat > sub/data.txt << HERE
foo
bar
baz
HERE
update

check_exist sub/foo sub/bar sub/baz
tup_object_exist sub 'touch foo bar baz'

eotup
