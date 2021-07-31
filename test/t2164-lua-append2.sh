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

# Test += with foo.bar inside a function

. ./tup.sh
cat > ok.sh << HERE
cat foo.h foo.c
touch output.txt
HERE
cat > Tupfile.lua << HERE
function ld_linux(objlist, obj)
	objlist.extra_inputs += 'foo.h'
	tup.rule(objlist, 'sh ok.sh', obj)
end

tup.rule('touch %o', {'foo.h'})
objlist = {'foo.c'}
obj = 'output.txt'
ld_linux(objlist, obj)
HERE
touch foo.c
update

eotup
