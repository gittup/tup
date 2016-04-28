#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2016  Mike Shal <marfey@gmail.com>
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

# Read data from a file using io.lines

. ./tup.sh
cat > data.txt << HERE
foo
bar
HERE
cat > Tupfile.lua << HERE
local outputs = {}
for line in io.lines('data.txt') do
	outputs += line
end
tup.rule({}, 'touch %o', outputs)
HERE
update

check_exist foo bar
tup_object_exist . 'touch foo bar'

cat > data.txt << HERE
foo
bar
baz
HERE
tup touch data.txt
update

check_exist foo bar baz
tup_object_exist . 'touch foo bar baz'

eotup
