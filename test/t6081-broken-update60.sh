#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018  Mike Shal <marfey@gmail.com>
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

# If a file has ghosts under it, then we delete the file and make it a directory, we end up with:
# SQL reset error: UNIQUE constraint failed: node.dir, node.name
# Statement was: insert into node(dir, type, name, display, flags, mtime, srcid) values(?, ?, ?, ?, ?, ?, ?)
. ./tup.sh
check_no_windows shell

mkdir -p subdir/foo
touch subdir/foo/bar
cat > Tupfile << HERE
: |> if [ -f subdir/foo/bar/baz.o ]; then echo "Yay"; else echo "ghost"; fi |>
HERE
update

rm -f subdir/foo/bar
mkdir subdir/foo/bar
touch subdir/foo/bar/3p.o
update

eotup
