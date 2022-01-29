#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2016-2022  Mike Shal <marfey@gmail.com>
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

# !tup_preserve has no effect without variants

. ./tup.sh

cat > Tupfile << HERE
: file.txt |> !tup_preserve |>
HERE

echo 'some content' > file.txt

# test that usage without variants don't result in errors
update

mkdir build-1
touch build-1/tup.config
mkdir build-2
touch build-2/tup.config

update

# test that the file contents have been preserved
cmp file.txt build-1/file.txt
cmp file.txt build-2/file.txt

# Make sure we can re-parse the Tupfile now that we have file.txt in the srcdir
# and the build dir.
touch Tupfile
update > .tup/.tupoutput
if grep 'preserve file.txt' .tup/.tupoutput > /dev/null; then
	cat .tup/.tupoutput
	echo "Error: No preserve commands should run when nothing was changed." 1>&2
	exit 1
fi

# test that the file contents have been preserved
cmp file.txt build-1/file.txt
cmp file.txt build-2/file.txt

eotup
