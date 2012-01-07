#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2012  Mike Shal <marfey@gmail.com>
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

# Make sure we can read in config vars with the monitor running.

. ./tup.sh
check_monitor_supported
tup monitor
cat > Tupfile << HERE
srcs-@(FOO) += foo.c
srcs-@(BAR) += bar.c
: foreach \$(srcs-y) |> gcc -c %f -o %o |> %B.o
HERE

touch foo.c bar.c
update
check_not_exist foo.o bar.o

varsetall FOO=y
update
check_not_exist bar.o
check_exist foo.o

varsetall BAR=y
update
check_not_exist foo.o
check_exist bar.o

eotup
