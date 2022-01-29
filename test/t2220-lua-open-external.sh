#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2020-2022  Mike Shal <marfey@gmail.com>
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

# Read from an external data file with lua, and update it with full_deps
# enabled.

. ./tup.sh
check_tup_suid

cat > data.txt << HERE
foo
bar
HERE
mkdir sub
cd sub
re_init
set_full_deps

cat > Tupfile.lua << HERE
local f = assert(io.open('../data.txt', 'r'))
local outputs = {}
for line in f:lines() do
	outputs += line
end
f:close()
tup.rule({}, 'touch %o', outputs)
HERE
update

check_exist foo bar

cat > ../data.txt << HERE
foo
bar
baz
HERE
update

check_exist foo bar baz

eotup
