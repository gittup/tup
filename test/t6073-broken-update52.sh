#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2017-2024  Mike Shal <marfey@gmail.com>
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

# Make a directory that is used in a rule, then remove the directory and
# replace it with a symlink. This failed because the ghost dir2 would be placed
# into the create_list, then the new dir2 file created over top would still be
# in the create list. Since the top-level directory has a dependency on dir2,
# it would try to parse it in parse_dependent_tupfiles(), which bypassed the
# TUP_NODE_DIR check from create_work().
. ./tup.sh

mkdir dir1
mkdir dir2
touch dir1/ok.in
cat > Tupfile << HERE
: dir2/ok.in |> cp %f %o |> %B.out
HERE

cd dir2
ln -s ../dir1/ok.in
cd ..

update

rm -rf dir2
ln -s dir1 dir2
parse_fail_msg "Explicitly named file 'ok.in' not found in subdir 'dir2'"

eotup
