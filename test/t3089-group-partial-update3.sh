#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2023  Mike Shal <marfey@gmail.com>
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

# Similar to t3088, but with multiple levels of groups, so we need to call
# expand_node() on the groups when building the graph.
. ./tup.sh

cat > Tupfile << HERE
: <group2> |> touch %o |> final.txt <final>
HERE
update_partial '<final>'

cat > Tupfile << HERE
: |> touch %o |> out.txt <group>
: <group2> |> touch %o |> final.txt <final>
HERE
update_partial '<final>'

check_not_exist out.txt

cat > Tupfile << HERE
: <group> |> touch %o |> out2.txt <group2>
: <group2> |> touch %o |> final.txt <final>
HERE
update_partial '<final>'

check_not_exist out.txt
check_exist out2.txt final.txt

cat > Tupfile << HERE
: |> touch %o |> out.txt <group>
: <group> |> touch %o |> out2.txt <group2>
: <group2> |> touch %o |> final.txt <final>
HERE
update_partial '<final>'

check_exist out.txt out2.txt final.txt

eotup
