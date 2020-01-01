#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2020  Mike Shal <marfey@gmail.com>
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

# Check to see if we can create a ghost node, then shutdown the monitor and
# turn the ghost into a symlink, then shutdown the monitor and change the
# symlink.
monitor
cat > Tupfile << HERE
: |> if [ -f ghost ]; then cat ghost; else echo nofile; fi > %o |> output.txt
HERE
update
stop_monitor
echo nofile | diff - output.txt
tup_object_exist . ghost

echo foo > bar
echo newfoo > baz
ln -s bar ghost
monitor
update
stop_monitor
echo foo | diff - output.txt

rm ghost
ln -s baz ghost
sleep 1
symtouch ghost
monitor
update
stop_monitor
echo newfoo | diff - output.txt

eotup
