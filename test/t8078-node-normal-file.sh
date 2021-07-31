#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2021  Mike Shal <marfey@gmail.com>
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

# Test that a node-variable can refer to a non-generated file
# in a variant build also after touching the Tupfile between two
# updates.

. ./tup.sh

mkdir build

cat > Tupfile << HERE
&lib = myLib.a
: &(lib) |> cp %f %o |> %b.copy
HERE

touch build/tup.config myLib.a
update
touch Tupfile
update

tup_dep_exist . myLib.a build 'cp myLib.a build/myLib.a.copy'

eotup
