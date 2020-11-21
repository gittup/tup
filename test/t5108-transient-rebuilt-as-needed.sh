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

# Make sure transient outputs are rebuilt when needed, even if their inputs
# haven't changed.

. ./tup.sh

cat > Tupfile << HERE
: |> ^t^ echo foo > %o |> tmp.txt
: |> cp input.txt %o |> bar.txt
: tmp.txt bar.txt |> cat %f > %o |> final.txt
HERE
echo 'bar' > input.txt
update

(echo foo; echo bar) | diff - final.txt
check_not_exist tmp.txt

echo 'blah' > input.txt
tup touch input.txt
update -j1
(echo foo; echo blah) | diff - final.txt
check_not_exist tmp.txt

eotup
