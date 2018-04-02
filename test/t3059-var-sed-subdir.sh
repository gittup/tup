#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2018  Mike Shal <marfey@gmail.com>
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

# Same as t3001, but in a sub-directory.

. ./tup.sh
check_no_ldpreload varsed
tmkdir sub
cat > sub/Tupfile << HERE
: foo.txt |> tup varsed %f %o |> out.txt
HERE
echo "hey @FOO@ yo" > sub/foo.txt
echo "This is an email@address.com" >> sub/foo.txt
tup touch sub/foo.txt sub/Tupfile
varsetall FOO=sup
update
tup_object_exist sub foo.txt out.txt
(echo "hey sup yo"; echo "This is an email@address.com") | diff sub/out.txt -

varsetall FOO=blah
update
(echo "hey blah yo"; echo "This is an email@address.com") | diff sub/out.txt -

eotup
