#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012  Mike Shal <marfey@gmail.com>
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

# Make sure that deleting a command will remove only its nodes from the group.

. ./tup.sh
cat > Tupfile << HERE
: |> touch foo |> foo <group>
: |> touch bar |> bar <group>
HERE
update

tup_dep_exist . foo . '<group>'
tup_dep_exist . bar . '<group>'

# Just remove 'touch foo'. Nothing from bar should be affected
cat > Tupfile << HERE
: |> touch bar |> bar <group>
HERE
tup touch Tupfile
update

tup_object_no_exist . foo
tup_dep_exist . bar . '<group>'

# Remove 'touch bar'. Now the group should be gone.
cat > Tupfile << HERE
HERE
tup touch Tupfile
update

tup_object_no_exist . '<group>'

eotup
