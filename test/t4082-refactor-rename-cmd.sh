#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2017  Mike Shal <marfey@gmail.com>
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

# Try the 'tup refactor' command, by changing a command string.

. ./tup.sh

cat > Tupfile << HERE
: |> touch foo |> foo
HERE
tup touch Tupfile
update

# Sleep 1 here so that we update the mtime in a transaction prior to
# parsing.
sleep 1
cat > Tupfile << HERE
string = foo
: |> touch \$(string) |> foo
HERE
refactor

cat > Tupfile << HERE
string = foo
: |> touch  \$(string) |> foo
HERE
tup touch Tupfile
refactor_fail_msg "Attempting to modify a command string:"
refactor_fail_msg "Old: 'touch foo'"
refactor_fail_msg "New: 'touch  foo'"

eotup
