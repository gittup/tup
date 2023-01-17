#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2023  Mike Shal <marfey@gmail.com>
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

# Try to remove an output from a command and see that the group is updated

. ./tup.sh
cat > ok.sh << HERE
touch foo
touch bar
HERE
cat > Tupfile << HERE
: |> sh ok.sh |> foo bar <group>
HERE
update

tup_dep_exist . 'foo' . '<group>'
tup_dep_exist . 'bar' . '<group>'
tup_dep_exist . 'sh ok.sh' . '<group>'

cat > ok.sh << HERE
touch foo
HERE
cat > Tupfile << HERE
: |> sh ok.sh |> foo <group>
: |> touch bar |> bar
HERE
update

tup_dep_exist . 'foo' . '<group>'
tup_dep_no_exist . 'bar' . '<group>'
tup_dep_exist . 'sh ok.sh' . '<group>'

eotup
