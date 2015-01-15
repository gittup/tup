#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2015  Mike Shal <marfey@gmail.com>
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

# This broke in commit 9df4109c16ac2c8075e73a8234d58a81e2cff44a - apparently I
# was relying on the watch to move when a directory moves.

. ./tup.sh
check_monitor_supported
monitor
mkdir a
touch a/foo.c
tup flush
mv a b
update

echo ': foreach *.c |> gcc -c %f -o %o |> %B.o' > b/Tupfile
update
tup_object_exist . b
tup_object_exist b foo.o
tup_object_no_exist . a
check_exist b/foo.o

eotup
