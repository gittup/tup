#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2018  Mike Shal <marfey@gmail.com>
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

# Verify the --verbose flag.

. ./tup.sh

tmp=".tmp.txt"
cat > Tupfile << HERE
: ok.c |> ^ short^ gcc -c %f -o %o |> %B.o
HERE
tup touch ok.c Tupfile
update > $tmp

if ! grep short $tmp > /dev/null; then
	echo "Error: Expected 'short' to be in the output text." 1>&2
	exit 1
fi
if grep 'gcc -c ok.c -o ok.o' $tmp > /dev/null; then
	echo "Error: Expected the gcc string to be absent from the output text." 1>&2
	exit 1
fi

tup touch ok.c
update --verbose > $tmp
if grep short $tmp > /dev/null; then
	echo "Error: Expected 'short' not to be in the output text." 1>&2
	exit 1
fi
if ! grep 'gcc -c ok.c -o ok.o' $tmp > /dev/null; then
	echo "Error: Expected the gcc string to be in the output text." 1>&2
	exit 1
fi

eotup
