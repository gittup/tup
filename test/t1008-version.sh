#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2024  Mike Shal <marfey@gmail.com>
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

# Make sure 'tup version', 'tup -v', and 'tup --version' all do the same thing.
. ./tup.sh

a=`tup version`
b=`tup -v`
c=`tup --version`
if [ "$a" != "$b" ]; then
	echo "Error: Versions(a, b) don't match." 1>&2
	exit 1
fi
if [ "$a" != "$c" ]; then
	echo "Error: Versions(a, c) don't match." 1>&2
	exit 1
fi

eotup
