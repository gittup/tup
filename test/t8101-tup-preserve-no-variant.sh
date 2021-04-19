#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2015-2021  Mike Shal <marfey@gmail.com>
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

# Use !tup_preserve without any variants.
. ./tup.sh
check_no_windows variant

tmkdir build

cat > Tupfile << HERE
: foreach *.html |> !tup_preserve |>
: |> touch %o |> gen.txt
HERE
tup touch Tupfile file.html file2.html
update

check_exist file.html file2.html gen.txt

tmkdir sub
tmkdir sub/bar
cat > sub/bar/Tupfile << HERE
: file2.html |> !tup_preserve |>
: |> touch %o |> gen2.txt
HERE
tup touch sub/bar/Tupfile sub/bar/file2.html
update

check_exist sub/bar/file2.html sub/bar/gen2.txt

eotup
