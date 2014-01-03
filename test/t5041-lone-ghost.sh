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

# Make sure a lone ghost in a directory doesn't end up killing the directory
# that it's in.

. ./tup.sh
tmkdir foo
cat > Tupfile << HERE
: |> (cat foo/ghost 2>/dev/null || echo nofile) > %o |> output.txt
HERE
tup touch Tupfile
update
echo nofile | diff - output.txt

cat > Tupfile << HERE
HERE
tup touch Tupfile
update

tup_object_exist . foo

eotup
