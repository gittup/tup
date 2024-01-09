#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2024  Mike Shal <marfey@gmail.com>
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

# Make sure ghost variables stay when setting all normal variables.

. ./tup.sh
cat > Tupfile << HERE
file-y = foo.c
file-@(GHOST) += bar.c
: foreach \$(file-y) |> cat %f > %o |> %B.o
HERE
echo hey > foo.c
echo yo > bar.c
varsetall FOO=3
update
tup_object_exist . "cat foo.c > foo.o"
tup_object_no_exist . "cat bar.c > bar.o"
tup_object_exist tup.config GHOST
tup_object_exist tup.config FOO
tup_dep_exist tup.config GHOST 0 .

# The GHOST variable should still exist and point to the directory
varsetall FOO=4
tup_object_exist tup.config GHOST
tup_dep_exist tup.config GHOST 0 .

eotup
