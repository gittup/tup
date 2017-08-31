#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2017  Mike Shal <marfey@gmail.com>
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

# Make sure commands are combined based on the display text if available.
. ./tup.sh

cat > Tupfile << HERE
: |> ^ TEST1^ cat input1.txt |>
: |> ^ TEST1^ cat input2.txt |>
: |> ^ TEST2^ cat input3.txt |>
HERE
tup touch input1.txt input2.txt input3.txt
update

tup graph . --combine > ok.dot
gitignore_good 'node.*TEST1.*2 commands' ok.dot
gitignore_good 'node.*TEST2\\n"' ok.dot

eotup
