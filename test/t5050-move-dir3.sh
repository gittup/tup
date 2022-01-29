#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2022  Mike Shal <marfey@gmail.com>
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

# Make sure when moving a directory, that any Tupfiles in that directory cause
# the dependent Tupfiles to be re-parsed. In this case, the top-level Tupfile
# should be re-parsed and fail because a/a2/Test.tup no longer exists.
. ./tup.sh
mkdir a
mkdir a/a2
echo 'x = 5' > a/a2/Test.tup
echo 'include a/a2/Test.tup' > Tupfile

update

mv a b
update_fail

echo 'include b/a2/Test.tup' > Tupfile
update

eotup
