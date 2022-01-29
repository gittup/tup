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

# Make sure we can make a symlink to a file with a path that also includes
# a symlink.

. ./tup.sh
check_no_windows symlink
check_no_ldpreload symlink-dir

mkdir foo
ln -s foo boo
touch foo/ok.txt
ln -s boo/ok.txt sym
cat > Tupfile << HERE
: |> cat sym |>
HERE
update
tup_dep_exist . sym . 'cat sym'
tup_dep_exist foo ok.txt . 'cat sym'
tup_dep_exist . boo . 'cat sym'

eotup
