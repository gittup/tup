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

# Try to move a directory out of tup, and make sure dependent Tupfiles are
# parsed.

. ./tup.sh
mkdir real
cd real
re_init
tmkdir sub
echo ': |> echo blah |>' > sub/Test.tup
echo 'include sub/Test.tup' > Tupfile
tup touch Tupfile sub/Test.tup
update

mv sub ..
tup rm sub
update_fail

eotup
