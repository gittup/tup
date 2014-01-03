#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2014  Mike Shal <marfey@gmail.com>
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

# Try to move a directory out of the way and then immediately move a new
# directory in its place. The monitor should still be running afterward, which
# is checked by stop_monitor.
. ./tup.sh
check_monitor_supported
mkdir tuptest
cd tuptest
re_init
monitor

mkdir foo
cd foo
echo 'int main(void) {return 0;}' > foo.c
echo ': foreach *.c |> gcc %f -o %o |> %B' > Tupfile
cd ..
cp -Rp foo bar
update

mv foo ..
mv bar foo
stop_monitor

eotup
