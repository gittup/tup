#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2015  Mike Shal <marfey@gmail.com>
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

# Make sure we require a sticky input for a group before using it as a
# resource file.
. ./tup.sh

cat > Tupfile << HERE
: |> touch %o |> foo <group>
: |> cat %<group>.res > %o |> files.txt
HERE
update_fail_msg "Unable to find group '<group>' as an input"

cat > Tupfile << HERE
: |> touch %o |> foo <group>
: <group> |> cat %<group>.res > %o |> files.txt
HERE
tup touch Tupfile
update

if ! grep 'foo' files.txt > /dev/null; then
	echo "Error: Expected 'foo' to be listed in files.txt" 1>&2
	exit 1
fi

eotup
