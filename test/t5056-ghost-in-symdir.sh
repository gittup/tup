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

# Make sure if we try to read from a ghost using a path with a symlink, we
# get the dependency on the symlink file.

. ./tup.sh
check_no_windows shell
check_no_ldpreload symlink-dir

mkdir foo
ln -s foo boo
cat > Tupfile << HERE
: |> if [ -f boo/ghost ]; then cat boo/ghost; else echo nofile; fi > %o |> out.txt
HERE
update
echo 'nofile' | diff - out.txt

mkdir bar
echo 'hey' > bar/ghost
rm boo
ln -s bar boo
tup touch boo
update
echo 'hey' | diff - out.txt

eotup
