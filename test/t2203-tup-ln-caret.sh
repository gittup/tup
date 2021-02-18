#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2016-2021  Mike Shal <marfey@gmail.com>
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

# Try the !tup_ln macro with a filename that has a caret.

. ./tup.sh

cat > Tupfile << HERE
: input1^.txt |> !tup_ln |> link1.txt
: input2^.^txt |> !tup_ln |> link2.txt
HERE
touch input1^.txt input2^.^txt
update

tup_dep_exist . "!tup_ln input1^.txt link1.txt" . link1.txt
tup_dep_exist . "!tup_ln input2^.^txt link2.txt" . link2.txt

eotup
