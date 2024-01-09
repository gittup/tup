#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2020-2024  Mike Shal <marfey@gmail.com>
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

# Make sure a file in the transient list doesn't trigger commands
# unnecessarily.

. ./tup.sh

cat > Tupfile << HERE
: in1.txt |> ^t^ cp %f %o |> tmp1.txt
: in2.txt |> ^t^ cp %f %o |> tmp2.txt
: tmp2.txt |> cp %f %o |> final2.txt
: tmp1.txt tmp2.txt |> cat %f > %o |> final.txt
HERE
echo foo > in1.txt
echo bar > in2.txt
update_partial final2.txt

check_not_exist tmp1.txt
check_exist tmp2.txt

update > .tup/.tupoutput
if grep 'cp tmp2.txt final2.txt' .tup/.tupoutput; then
	echo "Error: Shouldn't re-copy tmp2.txt to final2.txt" 1>&2
	exit 1
fi

eotup
