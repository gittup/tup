#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2016-2018  Mike Shal <marfey@gmail.com>
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

# Test the %D dirname variable in an output.

. ./tup.sh
cat > Tupfile << HERE
: foreach a.txt d/b.txt d/d/c.txt |> cp %f %o |> %D%B.tst
HERE
mkdir -p d/d
touch a.txt d/b.txt d/d/c.txt
update
tup_object_exist .   a.tst
tup_object_exist d   b.tst
tup_object_exist d/d c.tst

eotup
