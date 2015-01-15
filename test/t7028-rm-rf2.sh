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

. ./tup.sh
check_monitor_supported
monitor
mkdir a
echo ': |> echo "#define FOO 3" > %o |> foo.h' > a/Tupfile
mkdir b
echo ': foreach *.c | ../a/foo.h |> gcc -c %f -o %o |> %B.o' > b/Tupfile
touch b/foo.c
update
tup_sticky_exist a foo.h b 'gcc -c foo.c -o foo.o'

rm -rf a
update_fail_msg "Failed to find directory ID for dir"

echo ': foreach *.c |> gcc -c %f -o %o |> %B.o' > b/Tupfile
update

eotup
