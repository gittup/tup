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

# Use a ^t flag to mark transient outputs.

. ./tup.sh

cat > Tupfile << HERE
: |> ^t^ echo foo > tmp.txt |> tmp.txt
: tmp.txt |> cp %f %o |> final.txt
: tmp.txt |> cp %f %o |> final2.txt
HERE
update -j1

echo 'foo' | diff - final.txt
echo 'foo' | diff - final2.txt
check_not_exist tmp.txt

update_null "No files should have been recompiled during second update."

eotup
