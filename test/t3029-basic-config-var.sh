#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2018  Mike Shal <marfey@gmail.com>
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

# Same as t3000, only use the $(CONFIG_VAR) syntax.

. ./tup.sh
cat > Tupfile << HERE
file-y = foo.c
file-\$(CONFIG_BAR) += bar.c
: foreach \$(file-y) |> cat %f > %o |> %B.o
HERE
echo hey > foo.c
echo yo > bar.c
tup touch foo.c bar.c Tupfile
varsetall BAR=n
update
tup_object_exist . foo.c bar.c
tup_object_exist . "cat foo.c > foo.o"
tup_object_no_exist . "cat bar.c > bar.o"
vardict_exist BAR=n

varsetall BAR=y
update
tup_object_exist . foo.c bar.c
tup_object_exist . "cat foo.c > foo.o"
tup_object_exist . "cat bar.c > bar.o"
vardict_exist BAR=y

eotup
