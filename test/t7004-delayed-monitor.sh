#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2008-2012  Mike Shal <marfey@gmail.com>
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

# First create a foo.c program, then stop the monitor
tup monitor
cp ../testTupfile.tup Tupfile
echo "int main(void) {return 0;}" > foo.c
update
stop_monitor

# Now we make a change outside of the monitor's control (create a new file)
echo "void bar(void) {}" > bar.c
tup monitor
update
tup_object_exist . bar.c bar.o
sym_check prog bar
stop_monitor

sleep 1
echo "void bar2(void) {}" >> bar.c
tup monitor
update
sym_check prog bar bar2
stop_monitor

# Finally, delete a file outside of the monitor's control
rm bar.c
tup monitor
update
sym_check prog ^bar ^bar2
stop_monitor

eotup
