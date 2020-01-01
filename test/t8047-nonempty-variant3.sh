#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2020  Mike Shal <marfey@gmail.com>
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

# Try to move the tup.config to a srcdir accidentally.
. ./tup.sh
check_no_windows variant

tmkdir build
tmkdir sub

cat > sub/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
tup touch build/tup.config sub/foo.c sub/bar.c
update

check_exist build/sub/foo.o build/sub/bar.o

mv build/tup.config sub/tup.config
update_fail_msg "Variant directory must only contain a tup.config file"

mv sub/tup.config build/tup.config
update

check_exist build/sub/foo.o build/sub/bar.o

eotup
