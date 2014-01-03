#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2014  Mike Shal <marfey@gmail.com>
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

# Use the 'tup variant' command when a variant directory already exists and
# is not empty..
. ./tup.sh
check_no_windows variant

tmkdir build-debug
tmkdir configs

touch build-debug/foo

cat > Tupfile << HERE
ifeq (@(DEBUG),y)
: |> touch %o |> foo
endif
: |> touch %o |> bar
HERE
echo "CONFIG_DEBUG=y" > configs/debug.config
echo "" > configs/default.config
if tup variant configs/*.config 2>.tupoutput; then
	echo "Error: Expected 'tup variant' to fail, but didn't" 1>&2
	exit 1
fi

if grep "tup error: Variant directory 'build-debug' already exists and is not empty" .tupoutput > /dev/null; then
	echo "tup variant expected to fail, and failed for the right reason"
else
	echo "Error: Expected 'tup variant' to fail." 1>&2
	exit 1
fi

eotup
