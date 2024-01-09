#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2024  Mike Shal <marfey@gmail.com>
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

# Add a normal file to a generated dir.

. ./tup.sh

cat > Tupfile << HERE
: |> touch %o |> foo/bar/baz/out.txt
HERE
update

check_exist foo/bar/baz/out.txt

touch foo/bar/new.txt
update > .out.txt

for i in foo foo.*bar; do
	if ! grep "Converting $i to a normal directory" .out.txt > /dev/null; then
		cat .out.txt
		echo "Error: Expected $i to be converted." 1>&2
		exit 1
	fi
done

if grep "Converting foo/bar/baz to a normal directory" .out.txt; then
	cat .out.txt
	echo "Error: Expected foo/bar/baz to not be converted." 1>&2
	exit 1
fi

eotup
