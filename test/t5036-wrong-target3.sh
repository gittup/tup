#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2021  Mike Shal <marfey@gmail.com>
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

# Same as t5027, only the file we stomp is in a different directory.
. ./tup.sh

mkdir sub
cat > sub/Tupfile << HERE
: |> echo foo > %o |> file1
HERE
update
echo foo | diff - sub/file1

# Oops - accidentally overwrite file1
cat > Tupfile << HERE
: |> echo bar > sub/file1 ; touch file2 |> file2
HERE
update_fail

cat > Tupfile << HERE
: |> echo bar > %o |> file2
HERE
update
echo foo | diff - sub/file1
echo bar | diff - file2

eotup
