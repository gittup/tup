#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2020-2024  Mike Shal <marfey@gmail.com>
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

# Make sure 'tup options' works correctly if run in a subdir.
check()
{
	if tup options $3 | grep "$1.*$2" > /dev/null; then
		:
	else
		echo "Error: Expected option value $1 to be set to $2" 1>&2
		exit 1
	fi
}

. ./tup.sh

check keep_going 0
cat > .tup/options << HERE
[updater]
keep_going = 1
HERE
check keep_going 1
mkdir subdir
cd subdir
check keep_going 1

eotup
