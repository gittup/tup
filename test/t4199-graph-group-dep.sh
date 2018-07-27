#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018  Mike Shal <marfey@gmail.com>
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
HERE
tup touch input.txt
update

tup graph input.txt > ok.dot

gitignore_good '<group>' ok.dot
gitignore_good '<newgroup>' ok.dot
gitignore_good '<final>' ok.dot

eotup
