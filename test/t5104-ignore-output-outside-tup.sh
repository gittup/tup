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

# Make sure we can exclude an output file when it is outside of the tup hierarchy.

. ./tup.sh
mkdir tmp
cd tmp
re_init

cat > Tupfile << HERE
: |> sh run.sh |> output.txt ^/exclude.txt
HERE
cat > run.sh << HERE
touch output.txt
echo bar > ../exclude.txt
HERE
echo 'foo' > ../exclude.txt
update

echo 'bar' | diff - ../exclude.txt

eotup
