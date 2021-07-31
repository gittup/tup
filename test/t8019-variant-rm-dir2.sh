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

# Make sure just deleting the generated files doesn't result in the variant
# dirs going away.
. ./tup.sh

mkdir build
mkdir build-debug

mkdir sub
cat > sub/Tupfile << HERE
ifeq (@(DEBUG),y)
: |> touch %o |> foo
endif
: |> touch %o |> bar
HERE

echo "" > build/tup.config
echo "CONFIG_DEBUG=y" > build-debug/tup.config
touch build/tup.config build-debug/tup.config sub/Tupfile

update

tup_object_exist build sub
tup_object_exist build-debug sub

cat > sub/Tupfile << HERE
ifeq (@(DEBUG),y)
: |> touch %o |> foo
endif
HERE
touch sub/Tupfile
update

check_exist build/sub
check_exist build-debug/sub

tup_object_exist build sub
tup_object_exist build-debug sub

# ... and back again
cat > sub/Tupfile << HERE
ifeq (@(DEBUG),y)
: |> touch %o |> foo
endif
: |> touch %o |> bar
HERE
touch sub/Tupfile

update

check_exist build/sub
check_exist build-debug/sub

tup_object_exist build sub
tup_object_exist build-debug sub

eotup
