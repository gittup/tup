#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2014-2021  Mike Shal <marfey@gmail.com>
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

# Check %1f, %2f, %1o, etc

. ./tup.sh

cat > Tupfile << HERE
: file1.ext file2.ext foo/file3.ext file4.ext foo/file5.ext |> cmd %1f %3f %2o %1o %4B %5b |> out1 out2
HERE
mkdir foo
touch file1.ext file2.ext file4.ext
touch foo/file3.ext foo/file5.ext
parse

tup_object_exist . 'cmd file1.ext foo/file3.ext out2 out1 file4 file5.ext'

eotup
