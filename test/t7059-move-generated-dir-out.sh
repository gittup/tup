#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2021  Mike Shal <marfey@gmail.com>
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

# Make sure moving a directory out of tup will successfully remove watches on
# all the subdirectories. Use the USR1 signal to have the monitor quit if it
# has a watch on any invalid tupid.

. ./tup.sh
check_monitor_supported
mkdir tuptest
cd tuptest
re_init
monitor

mkdir sub
echo ': foreach *.c |> gcc -c %f -o %o |> ../foo/bar/%B.o' > sub/Tupfile
touch sub/foo.c
update

signal_monitor

mv foo ..
echo ': foreach *.c |> gcc -c %f -o %o |> ../foo2/bar/%B.o' > sub/Tupfile
tup flush
signal_monitor
update

tup_object_no_exist . foo
signal_monitor
stop_monitor

eotup
