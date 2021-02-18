#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2020-2021  Mike Shal <marfey@gmail.com>
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

# Try to use a 'cd' command in a generate script.

. ./tup.sh
check_no_windows shell

# 'tup generate' runs without a tup directory
rm -rf .tup

mkdir sub1
echo 'a' > sub1/dat.txt
mkdir sub2
echo 'b' > sub2/dat.txt
cat > Tupfile << HERE
: |> cd sub1; cat dat.txt > out.txt |> sub1/out.txt
: |> cd sub2; cat dat.txt > out.txt |> sub2/out.txt
HERE
generate $generate_script_name
./$generate_script_name
echo 'a' | diff - sub1/out.txt
echo 'b' | diff - sub2/out.txt

eotup
