#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2020  Mike Shal <marfey@gmail.com>
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

# Make sure we don't get a spurious error message if the top-level Tuprules.tup file is a ghost.
. ./tup.sh

tmkdir sub
cat > sub/Tupfile << HERE
include_rules
HERE
update

tup touch bar.txt
update > output.txt
if grep 'Tuprules.tup: No such file or directory' output.txt > /dev/null; then
	cat output.txt
	echo "Error: Expected no Tuprules.tup error message" 1>&2
	exit 1
fi

eotup
