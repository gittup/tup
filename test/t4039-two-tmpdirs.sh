#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2016  Mike Shal <marfey@gmail.com>
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

# Make sure accessing the same filename in two different tmp directories
# works.

. ./tup.sh

cat > ok.sh << HERE
mkdir tmp1
mkdir tmp2
echo foo > tmp1/ok.txt
echo bar > tmp2/ok.txt
cat tmp1/ok.txt tmp2/ok.txt > output.txt
rm tmp1/ok.txt
rm tmp2/ok.txt
rmdir tmp1
rmdir tmp2
HERE
chmod +x ok.sh

cat > Tupfile << HERE
: ok.sh |> ./%f |> output.txt
HERE
tup touch ok.sh Tupfile
update

(echo foo; echo bar) | diff - output.txt

eotup
