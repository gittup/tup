#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2021  Mike Shal <marfey@gmail.com>
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

# Test using a var to sed a file

. ./tup.sh
check_no_ldpreload varsed
cat > Tupfile << HERE
: foo.txt |> tup varsed %f %o |> out.txt
HERE
echo "hey @FOO@ yo" > foo.txt
echo "This is an email@address.com" >> foo.txt
touch foo.txt Tupfile
varsetall FOO=sup
update
tup_object_exist . foo.txt out.txt
tup_object_exist . "tup varsed foo.txt out.txt"
(echo "hey sup yo"; echo "This is an email@address.com") | diff out.txt -

cat > Tupfile << HERE
HERE
touch Tupfile
update

tup_object_no_exist . out.txt
tup_object_no_exist . "tup varsed foo.txt out.txt"

eotup
