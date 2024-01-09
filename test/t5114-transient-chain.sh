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

# Make sure transient command chains are pulled in as necessary.

. ./tup.sh

cat > Tupfile << HERE
: in1.txt |> ^t^ cp %f %o |> tmp1.txt
: tmp1.txt |> ^t^ cp %f %o |> tmp2.txt
: tmp2.txt |> ^t^ cp %f %o |> tmp3.txt
: tmp3.txt in3.txt |> ^t^ cat %f > %o |> tmp4.txt
: tmp4.txt |> ^t^ cp %f %o |> tmp5.txt
: tmp5.txt |> cp %f %o |> out.txt
HERE
echo foo > in1.txt
echo bar > in3.txt
update

check_not_exist tmp1.txt tmp2.txt tmp3.txt tmp4.txt tmp5.txt
(echo 'foo'; echo bar) | diff - out.txt

echo 'baz' > in1.txt
update

check_not_exist tmp1.txt tmp2.txt tmp3.txt tmp4.txt tmp5.txt
(echo 'baz'; echo bar) | diff - out.txt

echo 'ok' > in3.txt
update

check_not_exist tmp1.txt tmp2.txt tmp3.txt tmp4.txt tmp5.txt
(echo 'baz'; echo ok) | diff - out.txt

eotup
