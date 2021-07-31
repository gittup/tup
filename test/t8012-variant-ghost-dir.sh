#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2021  Mike Shal <marfey@gmail.com>
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

# Make sure if we have a dir in the variant that is a ghost, we can still
# create that directory in the src tree and have it work.
. ./tup.sh
check_no_windows shell

mkdir build

echo "" > build/tup.config

mkdir sub
cat > sub/Tupfile << HERE
: |> if [ -f ghost/foo ]; then cat ghost/foo; else echo nofile; fi > %o |> output.txt
HERE
touch build/tup.config sub/Tupfile
update

echo nofile | diff - build/sub/output.txt

mkdir sub/ghost
echo hey > sub/ghost/foo
update

echo hey | diff - build/sub/output.txt

eotup
