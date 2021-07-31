#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2021  Mike Shal <marfey@gmail.com>
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

# Try keep-going with a specific target

. ./tup.sh
single_threaded

echo "void bar(void) {}" > bar.c
echo "int main(void) {bork; return 0;}" > foo.c
echo "void zap(void) {}" > zap.c
cp ../testTupfile.tup Tupfile
update_fail -k foo.o zap.o
check_exist zap.o

eotup
