#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2024  Mike Shal <marfey@gmail.com>
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

# Make sure that the link from one Tupfile to another stays even if the first
# Tupfile has errors. This could be an issue based on the way I plan to delete
# links before parsing for 4009. I think the rollback in the updater will make
# this all magically work though.
. ./tup.sh

mkdir bar
cat > bar/Tupfile << HERE
: ../fs/*.o |> ld -r %f -o built-in.o |> built-in.o
HERE

mkdir fs
cat > fs/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE

echo "void ext3fs(void) {}" > fs/ext3.c
echo "void ext4fs(void) {}" > fs/ext4.c

update

tup_dep_exist . fs . bar

cat > bar/Tupfile << HERE
bork
: ../fs/*.o |> ld -r %f -o built-in.o |> built-in.o
HERE
update_fail
tup_dep_exist . fs . bar

eotup
