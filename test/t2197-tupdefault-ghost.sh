#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2016-2024  Mike Shal <marfey@gmail.com>
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

# Make sure we pick up Tupdefault if it's added after when the directories are
# first parsed.

. ./tup.sh
mkdir sub
mkdir sub/a
mkdir sub/b
mkdir sub/b/b2
mkdir sub/c
mkdir sub/c/c2

# Parse empty directories first
parse

# Now create Tupdefault files
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
