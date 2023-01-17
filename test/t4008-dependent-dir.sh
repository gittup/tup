#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2023  Mike Shal <marfey@gmail.com>
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

cat > Tupfile << HERE
: fs/*.o |> ld -r %f -o built-in.o |> built-in.o
HERE

mkdir fs
cat > fs/Tupfile << HERE
: foreach input/*.o |> cp %f %o |> %b
HERE

mkdir fs/input
cat > fs/input/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE

echo "void ext3fs(void) {}" > fs/input/ext3.c
echo "void ext4fs(void) {}" > fs/input/ext4.c

update

sym_check built-in.o ext3fs ext4fs

echo "void ext5fs(void) {}" > fs/input/ext5.c
update
sym_check built-in.o ext3fs ext4fs ext5fs

eotup
