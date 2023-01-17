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

# If the .gitignore file ends up in the modify list and we re-parse, it
# shouldn't bounce between a generated and normal file.

. ./tup.sh

cat > Tupfile << HERE
.gitignore
: |> touch foo |> foo
HERE
update

gitignore_good foo .gitignore

parse > .tup/output.txt 2>&1

if grep 'generated -> normal: .gitignore' .tup/output.txt > /dev/null; then
	cat .tup/output.txt
	echo "Error: .gitignore should not be converted to a normal file." 1>&1
	exit 1
fi

eotup
