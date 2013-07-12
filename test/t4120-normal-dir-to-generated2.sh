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

# Remove all the files in a normal dir, and create a new file in it from
# another directory. The directory should be converted to a generated dir.

. ./tup.sh

tmkdir foo
cat > foo/Tupfile << HERE
: |> touch %o |> ok.txt
HERE
update

rm foo/Tupfile
cat > Tupfile << HERE
: |> touch %o |> foo/bar.txt
HERE
update

check_exist foo

rm Tupfile
update

check_not_exist foo
tup_object_no_exist . foo

eotup
