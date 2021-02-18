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

# Try %d to refer to the directory name.

. ./tup.sh

cat > Tupfile.lua << HERE
tup.rule('echo %d')
HERE

tmkdir foo
cat > foo/Tupfile.lua << HERE
tup.rule('echo %d')
HERE

tmkdir bar
tmkdir bar/baz
cat > bar/Tupfile.lua << HERE
tup.rule('echo %d')
HERE
cat > bar/baz/Tupfile.lua << HERE
tup.rule('echo %d')
HERE

parse
tup_object_exist . 'echo tuptesttmp-t2146-lua-percd'
tup_object_exist foo 'echo foo'
tup_object_exist bar 'echo bar'
tup_object_exist bar/baz 'echo baz'

eotup
