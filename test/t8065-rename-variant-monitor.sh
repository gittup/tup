#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2017  Mike Shal <marfey@gmail.com>
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

# Try to rename a variant with the monitor running.
. ./tup.sh
check_no_windows variant
check_monitor_supported

monitor

mkdir build-foo
mkdir sub

cat > sub/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
touch sub/foo.c sub/bar.c
echo "CONFIG_FOO=y" > build-foo/tup.config
update

tup_object_exist build-foo/tup.config FOO

mv build-foo build-bar
update

tup_object_exist build-bar/tup.config FOO
check_exist build-bar/sub/foo.o
check_exist build-bar/sub/bar.o

stop_monitor
eotup
