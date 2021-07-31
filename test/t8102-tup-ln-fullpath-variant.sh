#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2016-2021  Mike Shal <marfey@gmail.com>
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

# Same as t2196, but in a variant.

. ./tup.sh
check_no_windows paths

mkdir sub
cat > sub/Tupfile << HERE
: `pwd -P`/foo/input.txt |> !tup_ln |> link.txt
HERE
mkdir foo
touch foo/input.txt

mkdir build
touch build/tup.config
update

tup_dep_exist build/sub "!tup_ln ../foo/input.txt ../build/sub/link.txt" build/sub link.txt

eotup
