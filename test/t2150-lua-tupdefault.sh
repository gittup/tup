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

# Allow a Tupdefault.lua file to be the default Tupfile.lua for
# all sub-directories.

. ./tup.sh
tmkdir sub
tmkdir sub/a
tmkdir sub/b
tmkdir sub/b/b2
tmkdir sub/c

cat > Tupdefault.lua << HERE
tup.rule('echo top')
HERE
cat > sub/Tupdefault.lua << HERE
tup.rule('echo hey')
HERE
cat > sub/c/Tupfile.lua << HERE
tup.rule('echo yo')
HERE
tup parse

tup_object_exist sub 'echo hey'
tup_object_exist sub/a 'echo hey'
tup_object_exist sub/b 'echo hey'
tup_object_exist sub/b/b2 'echo hey'
tup_object_exist sub/c 'echo yo'
tup_object_no_exist sub/c 'echo hey'
tup_object_no_exist . 'echo hey'

tup_object_exist . 'echo top'
tup_object_no_exist sub 'echo top'
tup_object_no_exist sub/a 'echo top'
tup_object_no_exist sub/b 'echo top'
tup_object_no_exist sub/b/b2 'echo top'
tup_object_no_exist sub/c 'echo top'

eotup
