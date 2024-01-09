#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2024  Mike Shal <marfey@gmail.com>
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

# Make sure we can do partial updates with either the command or the output, if
# the output is in a different directory.

. ./tup.sh

mkdir out1
mkdir out2

cat > out1/Tupfile << HERE
: ../input.txt |> cat %f > %o |> ../foo/out.txt
HERE
cat > out2/Tupfile << HERE
: ../input.txt |> cat %f > %o |> ../bar/out.txt
HERE
echo 'orig' > input.txt
update

# Try to partial update the directory containing the output
echo 'hey' > input.txt
update_partial foo

echo 'hey' | diff - foo/out.txt
echo 'orig' | diff - bar/out.txt

# Try to partial update the directory containing the command
echo 'yo' > input.txt
update_partial out1

echo 'yo' | diff - foo/out.txt
echo 'orig' | diff - bar/out.txt

# Try to partial update the generated file
echo 'new' > input.txt
update_partial foo/out.txt

echo 'new' | diff - foo/out.txt
echo 'orig' | diff - bar/out.txt

update

echo 'new' | diff - foo/out.txt
echo 'new' | diff - bar/out.txt

eotup
