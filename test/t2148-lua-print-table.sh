#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2018  Mike Shal <marfey@gmail.com>
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

# Automatically print tables in lua.

. ./tup.sh
cat > Tupfile.lua << HERE
CFLAGS += '-DFOO'
CFLAGS += '-DBAR'
print("My cflags are: ", CFLAGS)
HERE
tup touch Tupfile.lua
tup parse > .output.txt
if ! grep 'My cflags are:.*-DFOO -DBAR' .output.txt > /dev/null; then
	cat .output.txt
	echo "Error: Expecting CFLAGS to be expanded"
	exit 1
fi

eotup
