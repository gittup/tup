#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2021  Mike Shal <marfey@gmail.com>
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

# Make sure we inherit the correct sticky when changing command strings. This
# test has changed somewhat since its original intention.

. ./tup.sh
cat > Tupfile << HERE
: |> touch foo |> foo <group>
: <group> |> touch bar |> bar
: bar |> cat foo bar |>
HERE
tup touch Tupfile
update

tup_sticky_exist . '<group>' . 'touch bar'
tup_sticky_exist . '<group>' . 'cat foo bar'
tup_sticky_exist . bar . 'cat foo bar'

cat > Tupfile << HERE
: |> touch foo bar |> foo bar
: bar |> cat foo bar |>
HERE
tup touch Tupfile
update_fail_msg 'Missing input dependency'

tup_object_no_exist . '<group>'

eotup
