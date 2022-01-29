#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2022  Mike Shal <marfey@gmail.com>
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

# Make sure exclusions also ignore inputs.

. ./tup.sh

cat > Tupfile << HERE
: |> sh run.sh output.txt |> output.txt ^/ignore
HERE
cat > run.sh << HERE
cat ignore
cat ignoreghost 2>/dev/null
cat real
touch \$1
HERE
touch ignore real
update

tup_dep_exist . real . 'sh run.sh output.txt'
tup_dep_no_exist . ignore . 'sh run.sh output.txt'
tup_object_no_exist . ignoreghost

check_updates real output.txt
check_no_updates ignore output.txt

eotup
