#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2023  Mike Shal <marfey@gmail.com>
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

# Check that if a Tupfile stops using a variable, that var no longer has a
# dependency on the directory.

. ./tup.sh
mkdir tmp
cat > tmp/Tupfile << HERE
file-y = foo.c
file-@(BAR) += bar.c
: foreach \$(file-y) |> cat %f > %o |> %B.o
HERE
echo hey > tmp/foo.c
echo yo > tmp/bar.c
varsetall BAR=y
update
tup_object_exist tmp foo.c bar.c
tup_object_exist tmp "cat foo.c > foo.o"
tup_object_exist tmp "cat bar.c > bar.o"
tup_dep_exist tup.config BAR . tmp

cat > tmp/Tupfile << HERE
file-y = foo.c
: foreach \$(file-y) |> cat %f > %o |> %B.o
HERE
update
tup_object_exist tmp foo.c bar.c
tup_object_exist tmp "cat foo.c > foo.o"
tup_object_no_exist tmp "cat bar.c > bar.o"
tup_dep_no_exist tup.config BAR . tmp

eotup
