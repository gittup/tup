#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2008-2023  Mike Shal <marfey@gmail.com>
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

echo "int main(void) {}" > foo.c
update
sym_check foo.o main

echo "int main(void) {bork}" > foo.c
update_fail

echo "int main(void) {}" > foo.c
if [ "$(tup | grep -c 'gcc.*foo.o')" != 2 ]; then
	echo "foo.c should have been compiled and linked again." 1>&2
	exit 1
fi

eotup
