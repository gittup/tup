#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013  Mike Shal <marfey@gmail.com>
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

# Writing to a file from the lua parser should fail.

. ./tup.sh
cat > Tupfile.lua << HERE
local f = assert(io.open('data.txt', 'w'))
f:write('hey there')
f:close()
HERE
update_fail_msg 'io.open in the parser can only open files read-only'

eotup
