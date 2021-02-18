#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2021  Mike Shal <marfey@gmail.com>
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

# Verify that if we remove a normal directory containing a file generated from
# another directory, we end up re-creating the file.
. ./tup.sh

tmkdir sub
cat > sub/Tupfile << HERE
: |> touch %o |> ../out/foo.txt
HERE
tmkdir out
tup touch out/bar.txt
update

rm -rf out
update

check_exist out/foo.txt

# Make sure that 'out' is now a generated directory and is removed when the
# rule is removed.
cat > sub/Tupfile << HERE
HERE
tup touch sub/Tupfile
update

check_not_exist out

eotup
