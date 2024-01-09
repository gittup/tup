#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2024  Mike Shal <marfey@gmail.com>
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

# Test for the 'tup entry' window into the database
. ./tup.sh

entry()
{
	# sed is for some versions of grep on Windows that don't detect line
	# endings properly.
	if ! tup entry $1 | sed 's/\r$//' | grep "^$2$"; then
		echo "Expected entry $1 to be: $2" 1>&2
		exit 1
	fi
}

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
touch foo.c bar.c 1
update

entry foo.c foo.c
entry foo.o foo.o
entry 1 "\."
entry ./1 1

eotup
