#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2016-2018  Mike Shal <marfey@gmail.com>
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

# Try the !tup_ln macro with a full path input

. ./tup.sh
check_no_windows paths

tmkdir sub
cat > sub/Tupfile << HERE
: `pwd -P`/foo/input.txt |> !tup_ln |> link.txt
HERE
tmkdir foo
tup touch foo/input.txt
update

tup_dep_exist sub "!tup_ln `pwd -P`/foo/input.txt link.txt" sub link.txt

eotup
