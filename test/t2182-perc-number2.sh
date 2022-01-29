#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2014-2022  Mike Shal <marfey@gmail.com>
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

# Try some failure cases in %1f

. ./tup.sh
touch file1 file2
mkdir foo
touch foo/file3

cat > Tupfile << HERE
: file1 file2 foo/file3 |> cmd %0o |> out1 out2
HERE
parse_fail_msg "Expected number from 1-99"

cat > Tupfile << HERE
: file1 file2 foo/file3 |> cmd %100o |> out1 out2
HERE
parse_fail_msg "Expected number from 1-99"

cat > Tupfile << HERE
: file1 file2 foo/file3 |> cmd %1d |> out1 out2
HERE
parse_fail_msg "Expected.*after number in"

cat > Tupfile << HERE
: file1 |> cmd %1|> out1
HERE
parse_fail_msg "Unfinished %1-flag at the end of the string"

eotup
