#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2017  Mike Shal <marfey@gmail.com>
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

# Test using a node-variable as a rule input

. ./tup.sh
check_no_windows slashes

tmkdir sw
tmkdir sw/toolkit
tmkdir sw/app

cat > sw/Tuprules.tup << HERE
&toolkit_lib = toolkit/toolkit.a
HERE

cat > sw/app/Tupfile << HERE
include_rules
: &(toolkit_lib) |> cp %f %o |> %B.copy
HERE

tup touch sw/Tuprules.tup
tup touch sw/toolkit/toolkit.a
tup touch sw/app/Tupfile
update

tup_dep_exist sw/toolkit toolkit.a sw/app 'cp ../toolkit/toolkit.a toolkit.copy'

eotup
