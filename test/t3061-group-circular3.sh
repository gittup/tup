#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2021  Mike Shal <marfey@gmail.com>
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

# Similar to t6045, if we remove a group as an input, we have to delete any
# normal links on generated files and re-run the command.
#
# We can't just remove normal links for that group, since the group may have
# changed since we last used it (ie: the file we care about could have been
# removed from the group, but we still need to remove the normal link on it).

. ./tup.sh
cat > Tupfile << HERE
: |> cat input.txt > %o |> bar.txt | <group>
: <group> |> cat bar.txt > %o |> output.txt
HERE
touch input.txt
update

cat > Tupfile << HERE
: |> cat bar.txt > %o |> output.txt
: output.txt |> cat input.txt output.txt > %o |> bar.txt | <group>
HERE
touch Tupfile input.txt
update_fail_msg 'Missing input dependency'

eotup
