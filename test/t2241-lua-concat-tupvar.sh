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

# Set a variable in a Tupfile and then try to use concatenation in Lua.

. ./tup.sh

cat > Tupfile << HERE
myvar = ohai
include build.lua
HERE
cat > build.lua << HERE
tup.rule('echo ' .. myvar)
HERE
parse

tup_object_exist . 'echo ohai'

# Make sure it automatically becomes a string in an error message too.
cat > build.lua << HERE
error(myvar)
HERE
parse_fail_msg 'tup error ohai'

eotup
