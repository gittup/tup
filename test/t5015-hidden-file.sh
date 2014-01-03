#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2014  Mike Shal <marfey@gmail.com>
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

# Make sure we can't create hidden file nodes, or get dependencies from them
# in commands. The commands should still work to allow things like git describe
# to function, but in general reading from hidden files is discouraged.

. ./tup.sh
cat > Tupfile << HERE
: |> cat .hidden |>
HERE

echo 'foo' > .hidden
tmkdir yo
mkdir yo/.hidden_dir
if tup touch yo/.hidden_dir 2>/dev/null; then
	echo 'Error: tup-touching .hidden_dir should be an error' 1>&2
	exit 1
fi
echo 'bar' > yo/.hidden_dir/foo

if tup touch .hidden 2>/dev/null; then
	echo 'Error: tup-touching .hidden should be an error' 1>&2
	exit 1
fi
if tup touch yo/.hidden_dir/foo 2>/dev/null; then
	echo 'Error: tup-touching yo/.hidden_dir/foo should be an error' 1>&2
	exit 1
fi
tup touch Tupfile
update
tup_object_no_exist . .hidden
tup_object_exist . 'cat .hidden'

eotup
