#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2012  Mike Shal <marfey@gmail.com>
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

# Verify that output does not have the "Active" bar if we are redirecting to
# a file.
. ./tup.sh

cat > Tupfile << HERE
: |> echo foo |>
: |> echo bar 1>&2 |>
HERE

tup upd > out.txt
if grep Active out.txt > /dev/null; then
	echo "Error: Shouldn't see 'Active' in the output" 1>&2
	exit 1
fi

eotup
