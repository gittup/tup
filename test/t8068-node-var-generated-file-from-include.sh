#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2014  Mike Shal <marfey@gmail.com>
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

# Test that generated files can be referred to correctly from a Tupfile
# included through a node-variable, when using variants.
# (t2123 + variants)

. ./tup.sh
check_no_windows variant

tmkdir build
tmkdir sw
tmkdir lib

cat > Tuprules.tup << HERE
&lib_tupfile = lib/lib.tup
HERE

cat > lib/lib.tup << HERE
static_libs += \$(TUP_CWD)/lib.a
HERE

cat > lib/Tupfile << HERE
: foo.a |> cp %f %o |> lib.a
HERE

cat > sw/Tupfile << HERE
include_rules
include &(lib_tupfile)
: foreach \$(static_libs) |> cp %f %o |> %b.copy
HERE

tup touch Tuprules.tup lib/lib.tup lib/foo.a lib/Tupfile sw/Tupfile build/tup.config
update

tup_dep_exist build/lib lib.a build/sw 'cp ../lib/lib.a lib.a.copy'

eotup
