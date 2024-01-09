#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2008-2024  Mike Shal <marfey@gmail.com>
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

# Verify both files are compiled
echo "int main(void) {}" > foo.c
echo "void bar1(void) {}" > bar.c
update
sym_check foo.o main
sym_check bar.o bar1

# Verify only foo is compiled if foo.c is touched
echo "void foo2(void) {}" >> foo.c
if [ "$(tup | grep -c 'gcc -c')" != 1 ]; then
	echo "Only foo.c should have been compiled." 1>&2
	exit 1
fi
sym_check foo.o main foo2

# Verify both are compiled if both are touched, but only linked once
rm foo.o
touch foo.c bar.c
if [ "$(tup | grep -c 'gcc .* -o prog')" != 1 ]; then
	echo "Program should have only been linked once." 1>&2
	exit 1
fi
check_empty_tupdirs
sym_check foo.o main foo2
sym_check bar.o bar1

eotup
