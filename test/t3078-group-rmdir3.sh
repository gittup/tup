#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013  Mike Shal <marfey@gmail.com>
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

# Like t3067, but this time we have an input and output group,
# so the group_link table is used.
. ./tup.sh

tmkdir foo
tmkdir bar
cat > foo/Tupfile << HERE
: ../bar/<group> |> cp ../bar/file.txt %o |> copy.txt | ../bar/<output>
HERE
cat > bar/Tupfile << HERE
: |> echo hey > %o |> file.txt | <group>
HERE
update

# For some reason this issue takes several iterations to show up
for i in a b c; do
	rm -rf bar
	tup scan

	tmkdir bar
	cat > bar/Tupfile << HERE
: |> echo hey > %o |> file.txt | <group>
HERE
	update
done

eotup
