#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2016  Mike Shal <marfey@gmail.com>
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

# Try to use a Tupfile.ini to automatically call init when 'tup' is run.

. ./tup.sh

tmpdir="/tmp/tup-t4159"
cleanup()
{
	cd /tmp
	rm -rf $tmpdir
}

trap cleanup INT TERM
cleanup
mkdir $tmpdir
cd $tmpdir
output="$tupcurdir/$tuptestdir/output.txt"
touch Tupfile.ini
tup > $output

if ! grep 'Initializing .tup in.*tup-t4159' $output > /dev/null; then
	echo "Error: Expecting tup to initialize" 1>&2
	cleanup
	exit 1
fi

if ! grep 'Updated.' $output > /dev/null; then
	echo "Error: Expecting tup to update" 1>&2
	cleanup
	exit 1
fi

cleanup

cd $tupcurdir
eotup
