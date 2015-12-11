#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2015  Mike Shal <marfey@gmail.com>
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

# Allow a Tupdefault file to be the default Tupfile for
# all sub-directories.

. ./tup.sh
tmkdir sub
tmkdir sub/a
tmkdir sub/b
tmkdir sub/b/b2
tmkdir sub/c
tmkdir sub/c/c2

cat > Tupdefault.lua<< HERE
tup.rule('echo lua')
HERE
cat > Tupdefault<< HERE
: |> echo foo |>
HERE
cat > sub/a/Tupdefault<< HERE
: |> echo bar |>
HERE
cat > sub/c/Tupdefault.lua << HERE
tup.rule('echo yoyo')
HERE
cat > sub/c/Tupfile.lua << HERE
tup.rule('echo yo')
HERE
parse

tup_object_exist sub 'echo foo'
tup_object_no_exist sub 'echo bar'
tup_object_exist sub/a 'echo bar'
tup_object_no_exist sub/a 'echo foo'
tup_object_exist sub/b 'echo foo'
tup_object_exist sub/b/b2 'echo foo'
tup_object_exist sub/c 'echo yo'
tup_object_exist sub/c/c2 'echo yoyo'
tup_object_no_exist sub/c 'echo bar'
tup_object_no_exist sub/c 'echo foo'
tup_object_no_exist . 'echo yo'

tup_object_exist . 'echo foo'
tup_object_no_exist . 'echo lua'
tup_object_no_exist sub 'echo lua'
tup_object_no_exist sub/a 'echo lua'
tup_object_no_exist sub/b 'echo lua'
tup_object_no_exist sub/b/b2 'echo lua'
tup_object_no_exist sub/c 'echo lua'

eotup
