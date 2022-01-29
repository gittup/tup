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

# If a transient command produces two outputs and we only need one of them for
# an update, make sure both are still removed.

. ./tup.sh

cat > Tupfile << HERE
: |> ^t^ sh run.sh |> tmp1.txt tmp2.txt
: tmp1.txt |> cat in1.txt tmp1.txt > %o |> bar1.txt
: tmp2.txt |> cat in2.txt tmp2.txt > %o |> bar2.txt
HERE
cat > run.sh << HERE
echo tmp1 > tmp1.txt
echo tmp2 > tmp2.txt
HERE
touch in1.txt in2.txt
update

check_not_exist tmp1.txt tmp2.txt

# Try causing just one of the commands to run
touch in1.txt
update

check_not_exist tmp1.txt tmp2.txt

# Try causing both of the commands to run
touch in1.txt in2.txt
update

check_not_exist tmp1.txt tmp2.txt

eotup
