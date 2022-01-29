#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2022  Mike Shal <marfey@gmail.com>
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

# Try to output to a different dir, and then convert that dir to a file.

. ./tup.sh

cat > Tupfile << HERE
: |> echo text > %o |> foo/bar/baz
HERE
update

rm -rf foo/bar
touch foo/bar
# foo.bar.baz uses the . as a wildcard for matching Windows & Unix paths
update_fail_msg "Unable to output to a different directory because 'foo.bar' is a normal file"

eotup
