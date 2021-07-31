#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2021  Mike Shal <marfey@gmail.com>
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

# Same as t5071, but in a generated directory.

. ./tup.sh

mkdir sub
touch sub/ok.c
cat > sub/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> ../out/sub/%B.o
HERE
update

touch out/sub/ok.o
rm -rf sub/ok.c out
update
check_not_exist out/sub/ok.o
check_not_exist out/sub
check_not_exist out
tup_object_no_exist out/sub ok.o

eotup
