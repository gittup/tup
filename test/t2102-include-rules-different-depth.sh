#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2012  Mike Shal <marfey@gmail.com>
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

# Test that include_rules behaves properly when a Tupfile at a different
# folder depth is included.

. ./tup.sh

tmkdir sw
tmkdir sw/lib
tmkdir sw/app
tmkdir sw/app/core

cat > sw/lib/Tupfile << HERE
include_rules
vars += lib

ifeq (\$(TUP_CWD),.)
: |> !vars |> out.txt
endif
HERE

cat > sw/app/core/Tupfile << HERE
include_rules
include \$(lib_tupfile)
vars += core

ifeq (\$(TUP_CWD),.)
: |> !vars |> out.txt
endif
HERE

cat > sw/Tuprules.tup << HERE
lib_tupfile = \$(TUP_CWD)/lib/Tupfile
vars += sw
!vars = |> echo \$(vars) > %o |>
HERE
cat > sw/app/Tuprules.tup << HERE
vars += app
HERE

tup touch sw/lib/Tupfile sw/app/core/Tupfile sw/Tuprules.tup sw/app/Tuprules.tup
update

tup_dep_exist sw/lib 'echo sw lib > out.txt' sw/lib out.txt
tup_dep_exist sw/app/core 'echo sw app sw lib core > out.txt' sw/app/core out.txt

eotup
