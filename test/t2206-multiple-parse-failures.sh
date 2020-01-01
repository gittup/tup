#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2017-2020  Mike Shal <marfey@gmail.com>
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

# Make sure we get all error messages if there are multiple failures in the
# output list.

. ./tup.sh
tup touch foo bar
cat > Tupfile << HERE
: |> touch %o |> foo bar
HERE
parse_fail_msg "Attempting to insert 'foo'"
parse_fail_msg "Attempting to insert 'bar'"

cat > Tupfile << HERE
: |> touch %o |> out1 out1 out2 out2
HERE
parse_fail_msg "The output file 'out1' is listed multiple"
parse_fail_msg "The output file 'out2' is listed multiple"

cat > Tupfile << HERE
: |> touch %o |> Tuprules.tup Tupfile.lua
HERE
parse_fail_msg "Attempted to generate a file called 'Tuprules.tup'"
parse_fail_msg "Attempted to generate a file called 'Tupfile.lua'"

eotup
