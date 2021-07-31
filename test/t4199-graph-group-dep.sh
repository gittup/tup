#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2021  Mike Shal <marfey@gmail.com>
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

# Make sure 'tup graph' shows group dependencies.
. ./tup.sh

cat > Tupfile << HERE
: input.txt |> cp %f %o |> out.txt <group>
: <group> |> touch %o |> out2.txt <newgroup>
: <newgroup> |> touch %o |> out3.txt <final>
: <newgroup> |> touch %o |> out4.txt <final>
HERE
touch input.txt
update --debug-logging

tup graph input.txt > ok.dot
for i in ok.dot .tup/log/update.dot.0; do
	if cat $i | grep -- '->' | sort | uniq -c | awk '{print $1}' | grep 2 > /dev/null; then
		echo "Error: Shouldn't have more than one unique link in graph [$i]: " 1>&2
		cat $i 1>&2
		exit 1
	fi
done

gitignore_good '<group>' ok.dot
gitignore_good '<newgroup>' ok.dot
gitignore_good '<final>' ok.dot

eotup
