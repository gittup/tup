#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2012  Mike Shal <marfey@gmail.com>
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

# Test to make sure that if the monitor is stopped and re-started, we don't
# hose up existing flags.

. ./tup.sh
check_monitor_supported
monitor

echo "int main(void) {return 0;}" > foo.c
cp ../testTupfile.tup Tupfile
stop_monitor
monitor
update
tup_object_exist . foo.c foo.o prog.exe
sym_check foo.o main
sym_check prog.exe main

# Set foo.c's modify flags, then secretly remove foo.o behind the monitor's
# back (so we can see it gets re-created). When the monitor starts again, it
# shouldn't clear foo.c's flags.
touch foo.c
stop_monitor
rm foo.o
monitor
update
tup_object_exist . foo.c foo.o prog.exe
sym_check foo.o main
sym_check prog.exe main

# If we just stop and then start the monitor after an update, no flags should
# be set.
stop_monitor
monitor
check_empty_tupdirs

eotup
