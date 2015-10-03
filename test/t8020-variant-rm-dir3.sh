#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2021  Mike Shal <marfey@gmail.com>
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

# Make sure that we can clean up old variant directories, even if the
# Tupfiles are gone.
. ./tup.sh

tmkdir build

tmkdir sub
tmkdir sub/foo
echo ": |> touch %o |> hey" > sub/Tupfile
echo ": |> touch %o |> yo" > sub/foo/Tupfile
tup touch build/tup.config sub/Tupfile sub/foo/Tupfile

update

check_exist build/sub
check_exist build/sub/foo
tup_object_exist build sub
tup_object_exist build/sub foo

# Now removing the parent directory should keep the whole variant tree
rm sub/Tupfile
update

check_exist build/sub
check_exist build/sub/foo
tup_object_exist build sub
tup_object_exist build/sub foo

# Removing the last Tupfile is still allowed to have the corresponding
# variant directories around, though they aren't needed anymore.
rm sub/foo/Tupfile
update

check_exist build/sub
check_exist build/sub/foo
tup_object_exist build sub
tup_object_exist build/sub foo

# Now removing the actual srctree should result in the variant directories
# going away as well.
rm -rf sub
update

check_not_exist build/sub
check_not_exist build/sub/foo
tup_object_no_exist build sub
tup_object_no_exist build/sub foo

eotup
