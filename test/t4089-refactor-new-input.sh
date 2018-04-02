#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2018  Mike Shal <marfey@gmail.com>
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

# Try the 'tup refactor' command, by adding an input.

. ./tup.sh

cat > Tupfile << HERE
: |> touch bar |> bar
: |> touch baz |> baz
: |> echo baz |>
HERE
tup touch Tupfile
update

cat > Tupfile << HERE
: |> touch bar |> bar
: |> touch baz |> baz
: bar baz |> echo baz |>
HERE
tup touch Tupfile
refactor_fail_msg "Attempting to add a new input link: bar"
refactor_fail_msg "Attempting to add a new input link: baz"

eotup
