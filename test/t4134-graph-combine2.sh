#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013  Mike Shal <marfey@gmail.com>
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

# Try tup graph --combine with commands
. ./tup.sh

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> gcc %f -o %o |> prog.exe
HERE
tup touch foo.c bar.c baz.c
echo "int main(void) {return 0;}" > foo.c
update

tup touch *.c
tup scan
tup graph --combine > ok.dot
if ! grep 'node.*\.o.*3 files' ok.dot > /dev/null; then
	echo "Error: Expected objects to be combined" 1>&2
	exit 1
fi
if ! grep 'node.*\.c.*3 files' ok.dot > /dev/null; then
	echo "Error: Expected sources to be combined" 1>&2
	exit 1
fi
if ! grep 'node.*gcc -c.*3 commands' ok.dot > /dev/null; then
	echo "Error: Expected commands to be combined" 1>&2
	exit 1
fi

eotup
