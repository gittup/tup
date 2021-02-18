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

# Test that a Tupfile included through a node-variable
# can refer to a generated file.

. ./tup.sh
check_no_windows slashes

tmkdir lib

cat > lib/Tupfile << HERE
: |> touch %o |> lib.a
HERE

cat > lib/lib.tup << HERE
static_libs += \$(TUP_CWD)/lib.a
HERE

cat > Tupfile << HERE
&lib = lib/lib.tup
include &(lib)
: foreach \$(static_libs) |> cp %f %o |> %b.copy
HERE

tup touch Tupfile lib/Tupfile lib/lib.tup
update

tup_dep_exist lib lib.a . 'cp lib/lib.a lib.a.copy'

eotup
