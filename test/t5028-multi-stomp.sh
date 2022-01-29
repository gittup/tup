#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2022  Mike Shal <marfey@gmail.com>
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

# Stomp on multiple other files.
. ./tup.sh

cat > Tupfile << HERE
: |> echo foo > %o |> file1
: |> echo foo2 > %o |> file2
HERE
update
echo foo | diff - file1
echo foo2 | diff - file2

# Stomp stomp stomp
cat > Tupfile << HERE
: |> echo foo > %o |> file1
: |> echo foo2 > %o |> file2
: |> echo bar > file1 ; echo bar2 > file2; touch file3 |> file3
HERE
update_fail

cat > Tupfile << HERE
: |> echo foo > %o |> file1
: |> echo foo2 > %o |> file2
: |> echo bar > %o |> file3
HERE
update
echo foo | diff - file1
echo foo2 | diff - file2
echo bar | diff - file3

eotup
