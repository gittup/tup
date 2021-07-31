#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2020-2021  Mike Shal <marfey@gmail.com>
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

# Write to a generated file in another directory, then override it manually and
# remove the rule to create it. The file should still exist and the directory
# should be a normal directory.

. ./tup.sh

cat > Tupfile << HERE
: |> echo generated > %o |> sub/genfile.txt
HERE
update

rm Tupfile
echo 'manual' > sub/genfile.txt
update

if ! tup type sub | grep '^directory'; then
	echo "Error: Expected 'sub' to be a regular directory" 1>&2
	exit 1
fi
eotup
