#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2021-2023  Mike Shal <marfey@gmail.com>
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

# Make sure 'tup init -h' and 'tup init --help' print help info instead of
# creating a directory called '-h' / '--help'.
. ./tup.sh
tmpdir="/tmp/tup-t0005-$$"
cleanup()
{
	cd /tmp
	rm -rf $tmpdir
}
trap cleanup INT TERM
cleanup
mkdir $tmpdir
cd $tmpdir

tup init -h 2>/dev/null
if [ -d "-h" ]; then
	echo "Error: -h directory created." 1>&2
	cleanup
	exit 1
fi

tup init --help 2>/dev/null
if [ -d "--help" ]; then
	echo "Error: --help directory created." 1>&2
	cleanup
	exit 1
fi

cleanup
cd $tupcurdir
eotup
