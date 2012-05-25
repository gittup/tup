#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012  Mike Shal <marfey@gmail.com>
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

# Another test of moving the srcdir, this time it isn't used by the root directory.
. ./tup.sh
check_no_windows variant
check_monitor_supported

tup monitor

mkdir build
mkdir sub
mkdir sub/foo
mkdir sub/foo/baz

cat > sub/foo/baz/Tupfile << HERE
: foreach bar.c |> gcc -c %f -o %o |> %B.o
HERE
echo "CONFIG_FOO=y" > build/tup.config
touch sub/foo/baz/bar.c
update

check_exist build/sub/foo/baz/bar.o
check_not_exist sub/foo/baz/bar.o

mv sub newsub
update

check_exist build/newsub/foo/baz/bar.o
check_not_exist newsub/foo/baz/bar.o
check_not_exist build/sub

stop_monitor
eotup
