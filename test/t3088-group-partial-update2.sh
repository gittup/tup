#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2021  Mike Shal <marfey@gmail.com>
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

# If we do a partial update on a group, we would expect to run all commands
# that are part of input groups. In this example, if we do a partial update
# with both commands in the Tupfile, we get both outputs. However, if we
# partial update with a single command, and then add a new command to <group>
# and do a partial update with <final>, the command that outputs to <group> was
# not being run since the <final> group was never loaded in the DAG.
. ./tup.sh

cat > Tupfile << HERE
: |> touch %o |> out.txt <group>
: <group> |> touch %o |> final.txt <final>
HERE
update_partial '<final>'

check_exist out.txt final.txt

cat > Tupfile << HERE
HERE
touch Tupfile
update

cat > Tupfile << HERE
: <group> |> touch %o |> final.txt <final>
HERE
touch Tupfile
update_partial '<final>'

cat > Tupfile << HERE
: |> touch %o |> out.txt <group>
: <group> |> touch %o |> final.txt <final>
HERE
touch Tupfile
update_partial '<final>'

check_exist out.txt final.txt

eotup
