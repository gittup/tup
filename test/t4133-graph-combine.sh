#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013  Mike Shal <marfey@gmail.com>
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

# Try tup graph --combine
. ./tup.sh

cat > ok.sh << HERE
cat in1.txt in2.txt
touch out1.txt out2.txt out3.txt
HERE
cat > Tupfile << HERE
: |> sh ok.sh |> out1.txt out2.txt out3.txt
HERE
tup touch in1.txt in2.txt Tupfile
update

tup touch in1.txt in2.txt
tup graph --combine > ok.dot
gitignore_good 'node.*in.*2 files' ok.dot
gitignore_good 'node.*out.*3 files' ok.dot

eotup
