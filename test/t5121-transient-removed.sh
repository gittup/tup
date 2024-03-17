#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2024  Mike Shal <marfey@gmail.com>
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

# Make sure files that are in the transient list and then deleted don't cause
# problems.

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

cat > Tupfile << HERE
: |> echo foo > %o |> final.txt
: |> echo bar > %o |> final2.txt
HERE
update

eotup
