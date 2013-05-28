#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013  Mike Shal <marfey@gmail.com>
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

# Make sure we can't use an externally generated file from another directory.

. ./tup.sh

# Case 1: we create the generated file before trying to use it (foo is parsed
# before bar).
tmkdir foo
cat > foo/Tupfile << HERE
: |> echo hey > %o |> ../out/gen.txt
: ../out/gen.txt |> cat %f |>
HERE

tmkdir bar
cat > bar/Tupfile << HERE
: ../out/gen.txt |> cat %f |>
HERE
tup touch foo/Tupfile bar/Tupfile
update_fail_msg "Explicitly named file 'out/gen.txt' can't be listed as an input because it was generated from external directory 'foo'"

# case 2: The generated directory is not present when we try to use it as an
# input
cat > bar/Tupfile << HERE
: |> echo hey > %o |> ../out/gen.txt
: ../out/gen.txt |> cat %f |>
HERE

cat > foo/Tupfile << HERE
: ../out/gen.txt |> cat %f |>
HERE
tup touch foo/Tupfile bar/Tupfile
update_fail_msg "Expected node 'out' to be in directory '.', but it is not there."

# case 3: The generated file is not present when we try to use it as an input
tmkdir out
update_fail_msg "Explicitly named file 'gen.txt' not found in subdir 'out'"

eotup
