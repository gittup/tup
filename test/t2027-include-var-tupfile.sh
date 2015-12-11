#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2015  Mike Shal <marfey@gmail.com>
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

# See if we can include a Tupfile using a variable de-reference.

. ./tup.sh
cat > Tupfile << HERE
var = foo
include \$(var).tup
HERE

cat > foo.tup << HERE
: |> echo foo > %o |> file
HERE
cat > bar.tup << HERE
: |> echo bar > %o |> file
HERE

tup touch Tupfile foo.tup bar.tup
parse
tup_object_exist . 'echo foo > file'
tup_dep_exist . foo.tup 0 .
tup_dep_no_exist . bar.tup 0 .

cat > Tupfile << HERE
var = bar
include \$(var).tup
HERE

tup touch Tupfile
parse
tup_object_exist . 'echo bar > file'
tup_dep_no_exist . foo.tup 0 .
tup_dep_exist . bar.tup 0 .

eotup
