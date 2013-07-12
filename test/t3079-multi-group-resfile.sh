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

# Try to use multiple groups on the command line at once.
. ./tup.sh

cat > Tupfile << HERE
: |> touch %o |> foo <group>
: |> touch %o |> bar <group2>
: <group> <group2> |> (for i in \`cat %<group>.res\`; do echo "Group1: \$i"; done; for i in \`cat %<group2>.res\`; do echo "Group2: \$i"; done; for i in \`cat %<group>.res\`; do echo "Group1again: \$i"; done) > %o |> files.txt
HERE
update

if ! grep 'Group1: foo' files.txt > /dev/null; then
	echo "Error: Expected Group1: foo" 1>&2
	exit 1
fi

if ! grep 'Group2: bar' files.txt > /dev/null; then
	echo "Error: Expected Group2: bar" 1>&2
	exit 1
fi

if ! grep 'Group1again: foo' files.txt > /dev/null; then
	echo "Error: Expected Group1again: foo" 1>&2
	exit 1
fi

eotup
