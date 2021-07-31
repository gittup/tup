#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2014-2021  Mike Shal <marfey@gmail.com>
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

# Output to a file in the root directory with a group, then move the directory.
# This causes the directory to be seen as deleted during the scan, so the
# command is immediately deleted. However, the output is not deleted because it
# exists in the root directory. When the new command is created and picks up
# the output, it tries to create the same group link and fails. Instead we
# should detect that it has an old group and prevent the double insertion.
. ./tup.sh

mkdir a
cat > Tuprules.tup << HERE
TOP = \$(TUP_CWD)
!cc = |> gcc -c %f -o %o |> \$(TOP)/%B.o \$(TOP)/<group>
HERE
cat > a/Tupfile << HERE
include_rules

: foreach *.c |> !cc |>
HERE
touch a/foo.c
update

mv a b
update

eotup
