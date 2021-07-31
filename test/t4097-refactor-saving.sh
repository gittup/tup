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

# 'tup refactor' should unflag the create bit on directories, so things that
# are refactored properly don't have to keep being reparsed.

. ./tup.sh

mkdir foo
mkdir bar
cat > foo/Tupfile << HERE
: |> echo foo |>
HERE
cat > bar/Tupfile << HERE
: |> echo bar |>
HERE
update

# Change both foo and bar Tupfiles, so they are both refactored.
cat > foo/Tupfile << HERE
str = foo
: |> echo \$(str) |>
HERE
cat > bar/Tupfile << HERE
str = bar
: |> echo \$(str) |>
HERE
refactor

# Now change just foo - only foo should be refactored, since bar
# was refactored successfully above.
cat > foo/Tupfile << HERE
cmd = echo
str = foo
: |> \$(cmd) \$(str) |>
HERE
tup refactor > .out.txt

if ! grep foo .out.txt > /dev/null; then
	echo "Error: 'foo' should be refactored" 1>&2
	exit 1
fi
if grep bar .out.txt > /dev/null; then
	echo "Error: 'bar' shouldn't be refactored" 1>&2
	exit 1
fi

eotup
