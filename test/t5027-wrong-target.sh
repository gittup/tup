#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2016  Mike Shal <marfey@gmail.com>
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

# If we have two commands, they obviously have to write to different files.
# However, if one of them behaves poorly and overwrites the other guy's output,
# we should automatically re-run the first command.
. ./tup.sh

cat > Tupfile << HERE
: |> echo foo > %o |> file1
HERE
tup touch Tupfile
update
echo foo | diff - file1

# Oops - accidentally overwrite file1
cat > Tupfile << HERE
: |> echo foo > %o |> file1
: |> echo bar > file1 ; touch file2 |> file2
HERE
tup touch Tupfile
update_fail

cat > Tupfile << HERE
: |> echo foo > %o |> file1
: |> echo bar > %o |> file2
HERE
tup touch Tupfile
update
echo foo | diff - file1
echo bar | diff - file2

eotup
