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

# Test reassigning a node variable.

. ./tup.sh

mkdir sw
mkdir sw/toolkit
mkdir sw/app

cat > sw/Tuprules.tup << HERE
&toolkit_lib = toolkit/toolkit.a
HERE

cat > sw/app/Tupfile << HERE
include_rules
&toolkit_lib = app.a
: &(toolkit_lib) |> cp %f %o |> %B.copy
HERE

touch sw/Tuprules.tup
touch sw/toolkit/toolkit.a
touch sw/app/Tupfile sw/app/app.a
update

tup_dep_exist sw/app app.a sw/app 'cp app.a app.copy'

eotup
