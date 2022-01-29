#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2022  Mike Shal <marfey@gmail.com>
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

# If we remove the generated directory and the src directory at the same time,
# we shouldn't get an invalid tupid in the create list.

. ./tup.sh

mkdir obj
mkdir obj/bar
cat > obj/bar/Tupfile << HERE
: |> echo hey > %o |> ../aaa/baz.txt
: |> echo yo > %o |> foo.txt
HERE
update

rm -rf obj
update

tup_object_no_exist . obj

eotup
