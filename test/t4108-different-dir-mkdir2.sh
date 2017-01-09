#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2017  Mike Shal <marfey@gmail.com>
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

# Manually remove the generated directory and make sure it gets re-created.

. ./tup.sh

cat > Tupfile << HERE
: |> touch %o |> foo/out.txt
HERE
tup touch Tupfile
update

check_exist foo/out.txt

# Manually rm the generated directory
rm -rf foo
update

check_exist foo/out.txt

# Try again and remove the rule so the directory stays gone.
rm -rf foo
cat > Tupfile << HERE
HERE
tup touch Tupfile
update

check_not_exist foo
tup_object_no_exist . foo

eotup
