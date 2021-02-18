#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2021  Mike Shal <marfey@gmail.com>
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

# Make sure 'tup init' returns a 1 if a .tup dir already exists, and 0 if we
# force.
. ./tup.sh

if tup init foo/bar/baz; then
	echo "Error: Expected 'tup init' to fail" 1>&2
	exit 1
fi

if ! tup init --force foo/bar/baz; then
	echo "Error: Expected 'tup init --force' to succeed" 1>&2
	exit 1
fi
if [ ! -f "foo/bar/baz/.tup/db" ]; then
	echo "foo/bar/baz/.tup/db not created!" 1>&2
	exit 1
fi

eotup
