#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2008-2014  Mike Shal <marfey@gmail.com>
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
cp ../testTupfile.tup Tupfile

# Since the 'foreach *.c' in the Tupfile will process the files in alphabetical
# order, these files should be built in the order (bar.c, foo.c, zap.c). Then
# we make the middle file break in order to test the keep-going logic.

single_threaded
echo "void bar(void) {}" > bar.c
echo "int main(void) {bork; return 0;}" > foo.c
echo "void zap(void) {}" > zap.c
tup touch bar.c foo.c zap.c
update_fail

check_exist bar.o
check_not_exist foo.o zap.o prog

update_fail -k
check_exist bar.o zap.o
check_not_exist foo.o prog

eotup
