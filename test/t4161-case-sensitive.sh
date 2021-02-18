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

# Make sure if we change the case of an output file, it gets re-created with
# the new case (even on case-insensitive platforms like Windows).

. ./tup.sh

cat > Tupfile << HERE
: |> echo text > %o |> output.txt
HERE
update

if ! ls | grep 'output.txt' > /dev/null; then
	echo "Error: Expected 'output.txt' in the filesystem" 1>&2
	exit 1
fi

cat > Tupfile << HERE
: |> echo text > %o |> OUTput.txt
HERE
tup touch Tupfile
update

if ls | grep 'output.txt' > /dev/null; then
	echo "Error: Expected no 'output.txt' in the filesystem" 1>&2
	exit 1
fi
if ! ls | grep 'OUTput.txt' > /dev/null; then
	echo "Error: Expected 'OUTput.txt' in the filesystem" 1>&2
	exit 1
fi

eotup
