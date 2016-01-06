#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2016  Mike Shal <marfey@gmail.com>
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

# Test files that begin with '.' but aren't ignored by tup.

. ./tup.sh
cat > Tupfile << HERE
: |> cat .hidden |>
HERE

echo 'foo' > .hidden
tmkdir yo
mkdir yo/.hidden_dir
tup touch yo/.hidden_dir
echo 'bar' > yo/.hidden_dir/foo

tup touch .hidden
tup touch yo/.hidden_dir/foo
tup touch Tupfile
update
tup_object_exist . .hidden
tup_object_exist . 'cat .hidden'
tup_dep_exist . .hidden . 'cat .hidden'

eotup
