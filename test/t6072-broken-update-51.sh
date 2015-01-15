#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2014-2015  Mike Shal <marfey@gmail.com>
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

# Output to a file in another directory, then blow away the directory that
# contained the rule. In this case, instead of re-parsing, we go through a
# recurse tup_del_id_type call which forces the directory and command nodes to
# go away. But since it wasn't reparsing, the directory sat around as a ghost,
# and then generated node still existed.
. ./tup.sh

tmkdir a
cat > Tuprules.tup << HERE
TOP = \$(TUP_CWD)
!cc = |> gcc -c %f -o %o |> \$(TOP)/%B.o \$(TOP)/<group>
HERE
cat > a/Tupfile << HERE
include_rules

: foreach *.c |> !cc |>
HERE
tup touch a/foo.c
update

rm -rf a
update

check_not_exist foo.o
tup_object_no_exist . a
tup_object_no_exist . foo.o

eotup
