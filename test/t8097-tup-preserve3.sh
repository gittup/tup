#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2015-2024  Mike Shal <marfey@gmail.com>
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

# Copy files from the srcdir to the variant dir.
. ./tup.sh

mkdir build

cat > Tupfile << HERE
: foreach *.html |> !tup_preserve |>
: |> touch %o |> gen.txt
HERE
echo file1 > file.html
echo file2 > file2.html
touch build/tup.config
update

check_exist build/file.html build/file2.html build/gen.txt
echo file1 | diff - build/file.html
echo file2 | diff - build/file2.html

mkdir sub
mkdir sub/bar
cat > sub/bar/Tupfile << HERE
: file3.html |> !tup_preserve |>
: |> touch %o |> gen2.txt
HERE
echo file3 > sub/bar/file3.html
update

check_exist build/sub/bar/file3.html build/sub/bar/gen2.txt
echo file3 | diff - build/sub/bar/file3.html

eotup
