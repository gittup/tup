#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2020-2023  Mike Shal <marfey@gmail.com>
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

# Make sure updating the transient command itself causes it and downstream
# things to rebuild.

. ./tup.sh

cat > Tupfile << HERE
: |> ^t^ cp in1.txt %o |> tmp.txt
: tmp.txt |> cp %f %o |> bar.txt
HERE
echo in1 > in1.txt
echo in2 > in2.txt
update

check_not_exist tmp.txt
echo 'in1' | diff - bar.txt

cat > Tupfile << HERE
: |> ^t^ cp in2.txt %o |> tmp.txt
: tmp.txt |> cp %f %o |> bar.txt
HERE
update

check_not_exist tmp.txt
echo 'in2' | diff - bar.txt

update_null "No files left to update"

eotup
