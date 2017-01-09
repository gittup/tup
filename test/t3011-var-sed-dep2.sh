#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2017  Mike Shal <marfey@gmail.com>
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

# Similar to t3009, only make sure that if the variable is deleted the command
# is still executed.

. ./tup.sh
cat > Tupfile << HERE
: foo.txt |> tup varsed %f %o |> out.txt
: out.txt |> cat %f > %o |> new.txt
HERE
echo "hey @FOO@ yo" > foo.txt
tup touch foo.txt Tupfile
varsetall FOO=sup
update
tup_object_exist . foo.txt out.txt new.txt
(echo "hey sup yo") | diff out.txt -
(echo "hey sup yo") | diff new.txt -

varsetall
update
(echo "hey  yo") | diff out.txt -
(echo "hey  yo") | diff new.txt -

varsetall FOO=diggity
update
(echo "hey diggity yo") | diff out.txt -
(echo "hey diggity yo") | diff new.txt -

eotup
