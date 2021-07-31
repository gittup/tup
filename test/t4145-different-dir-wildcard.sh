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

# Make sure we can only wildcard things from different directories where we
# create outputs there.

. ./tup.sh

cat > Tupfile << HERE
: |> touch %o |> foo/bar/out.txt
: foo/bar/*.txt foo/baz/*.txt |> cat %f > %o |> output.txt
HERE
update_fail_msg "Failed to find directory ID for dir 'foo/baz/\*.txt' relative to '.'"

rm Tupfile

mkdir sub
cat > sub/Tupfile << HERE
: |> touch %o |> ../foo/baz/out.txt
HERE
update

cat > Tupfile << HERE
: |> touch %o |> foo/bar/out.txt
: foo/bar/*.txt foo/baz/*.txt |> cat %f > %o |> output.txt
HERE
update_fail_msg 'Unable to use inputs from a generated directory'

eotup
