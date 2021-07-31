#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2008-2021  Mike Shal <marfey@gmail.com>
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

# Make sure the group updates when we remove a command that was adding files to
# a group.
. ./tup.sh

cat > Tupfile << HERE
: |> touch %o |> foo | <group>
: |> touch %o |> bar | <group>
: <group> |> echo %<group> > %o |> test.txt
HERE
touch Tupfile
update

echo 'foo bar' | diff - test.txt

cat > Tupfile << HERE
: |> touch %o |> foo | <group>
: <group> |> echo %<group> > %o |> test.txt
HERE
touch Tupfile
update

echo 'foo' | diff - test.txt

eotup
