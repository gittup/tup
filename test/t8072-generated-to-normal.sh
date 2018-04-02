#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2018  Mike Shal <marfey@gmail.com>
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

# Same as t6053, but in a variant.

. ./tup.sh
check_no_windows variant

mkdir build
touch build/tup.config

cat > Tupfile << HERE
: |> echo generated > %o |> genfile.txt
: genfile.txt |> cat %f > %o |> output.txt
HERE
tup touch Tupfile
update

echo 'generated' | diff - build/output.txt

cat > Tupfile << HERE
: genfile.txt |> cat %f > %o |> output.txt
HERE
echo 'manual' > genfile.txt
tup touch genfile.txt Tupfile
update

echo 'manual' | diff - build/output.txt

eotup
