#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2017  Mike Shal <marfey@gmail.com>
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

# Make sure a deleted src directory gets propagated to all variants.
. ./tup.sh
check_no_windows variant

tmkdir build
tmkdir build-debug

tmkdir sub
cat > sub/Tupfile << HERE
ifeq (@(DEBUG),y)
: |> touch %o |> foo
endif
: |> touch %o |> bar
HERE

echo "" > build/tup.config
echo "CONFIG_DEBUG=y" > build-debug/tup.config
tup touch build/tup.config build-debug/tup.config sub/Tupfile

update

tup_object_exist build sub
tup_object_exist build-debug sub

rm -rf sub
update

# Both the actual directories and the nodes in the db should be gone.
check_not_exist build/sub
check_not_exist build-debug/sub

tup_object_no_exist build sub
tup_object_no_exist build-debug sub

# ... and back again
tmkdir sub
cat > sub/Tupfile << HERE
ifeq (@(DEBUG),y)
: |> touch %o |> foo
endif
: |> touch %o |> bar
HERE
tup touch sub/Tupfile
update
tup_object_exist build sub
tup_object_exist build-debug sub

eotup
