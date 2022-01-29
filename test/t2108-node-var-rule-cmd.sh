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

# Test using a node-variable in a rule command line.

. ./tup.sh

mkdir sw
mkdir sw/toolkit
mkdir sw/app

cat > sw/Tuprules.tup << HERE
&toolkit_lib = toolkit/toolkit.a
HERE

cat > sw/app/Tupfile << HERE
include_rules
: |> cp &(toolkit_lib) %o |> lib_copy.a
HERE

touch sw/toolkit/toolkit.a
update

tup_dep_exist sw/toolkit toolkit.a sw/app "cp ../toolkit/toolkit.a lib_copy.a"

eotup
