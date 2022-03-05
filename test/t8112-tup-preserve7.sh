#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2022  Mike Shal <marfey@gmail.com>
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

# Use !tup_preserve with extra path elements on the output.
. ./tup.sh

mkdir build
mkdir sub

cat > sub/Tupfile << HERE
: foreach *.html |> !tup_preserve |> ../%b
: foreach *.html |> !tup_preserve |> %b
HERE
echo text > sub/file.html
touch build/tup.config
update

echo text | diff - build/file.html
echo text | diff - build/sub/file.html

eotup
