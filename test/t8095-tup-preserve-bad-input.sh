#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2016-2024  Mike Shal <marfey@gmail.com>
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

# Make sure !tup_preserve fails if given the output of another command.

. ./tup.sh

mkdir sub
cat > sub/Tupfile << HERE
: |> touch %o |> foo.txt
HERE
echo 'bar' > sub/bar.txt
mkdir build
touch build/tup.config

update

cat > sub/Tupfile << HERE
: |> touch %o |> foo.txt
: foo.txt |> !tup_preserve |>
HERE

update_fail_msg "Explicitly named file 'foo.txt' not found in subdir 'sub'"

cat > sub/Tupfile << HERE
: |> touch %o |> foo.txt
: bar.txt |> !tup_preserve |>
HERE
update

cmp sub/bar.txt build/sub/bar.txt

eotup
