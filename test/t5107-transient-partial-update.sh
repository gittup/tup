#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2020-2022  Mike Shal <marfey@gmail.com>
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

# Make sure transient outputs get cleaned up properly when updates only
# partially complete.

. ./tup.sh

cat > Tupfile << HERE
: |> ^t^ sh run.sh |> tmp1.txt tmp2.txt
: tmp1.txt |> cp %f %o |> final.txt
: tmp2.txt |> cp %f %o |> final2.txt
HERE
cat > run.sh << HERE
echo foo > tmp1.txt
echo bar > tmp2.txt
HERE

# If we haven't created either final output yet, both files should exist.
update_partial tmp1.txt
check_exist tmp1.txt tmp2.txt

# With only 1 final output created, the 2nd tmp file should still exist.
update_partial final.txt
check_not_exist tmp1.txt
check_exist tmp2.txt

# Once the last file is complete, the tmp files should all be gone.
update_partial final2.txt
check_not_exist tmp1.txt tmp2.txt

echo 'foo' | diff - final.txt
echo 'bar' | diff - final2.txt

update_null "No files left to update after all partial builds"

eotup
