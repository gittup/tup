#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2021  Mike Shal <marfey@gmail.com>
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

# Test for 'tup commandline' to print commandlines without a separate
# compile_commands.json

. ./tup.sh

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
tup touch foo.c bar.c
tup parse

tup commandline foo.c > .tup/output.txt

if ! grep '"command": "gcc -c foo.c -o foo.o",' .tup/output.txt > /dev/null; then
	echo "Error: Expected gcc command in output." 1>&2
	exit 1
fi

eotup
