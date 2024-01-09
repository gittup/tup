#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2024  Mike Shal <marfey@gmail.com>
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

mkdir -p foo/bar
cd foo/bar
echo 'int main(void) {return 0;}' > foo.c
echo ': foreach *.c |> gcc %f -o %o |> %B' > Tupfile
cd ../..
update

signal_monitor

mv foo ..
tup flush
signal_monitor
stop_monitor
update
tup_object_no_exist . foo

eotup
