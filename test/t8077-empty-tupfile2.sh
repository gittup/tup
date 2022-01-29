#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2022  Mike Shal <marfey@gmail.com>
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

# Same as t8076, but with the monitor running (so the monitor sets srcid == -2)

. ./tup.sh
check_monitor_supported

monitor

mkdir foo
mkdir foo/bar
touch foo/bar/Tupfile
mkdir build-debug
mkdir build-debug2
touch build-debug/tup.config
touch build-debug2/tup.config
update

rm -rf foo/bar
update

tup_object_no_exist . foo/bar
tup_object_no_exist build-debug foo/bar
tup_object_no_exist build-debug2 foo/bar

eotup
