#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2023  Mike Shal <marfey@gmail.com>
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

# Try to use the autoparse option.

. ./tup.sh
check_monitor_supported
monitor --autoparse

touch foo.c
cat > Tupfile << HERE
: foo.c |> gcc -c %f -o %o |> %B.o
HERE
tup flush
check_not_exist foo.o
tup_object_exist . 'gcc -c foo.c -o foo.o'

update
check_exist foo.o

rm Tupfile
tup flush
check_not_exist foo.o

stop_monitor

eotup
