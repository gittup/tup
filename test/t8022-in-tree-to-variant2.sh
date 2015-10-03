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

# Start with an in-tree build, then create a variant, then create another variant.
. ./tup.sh

tmkdir build

cat > Tupfile << HERE
ifeq (@(DEBUG),y)
: |> touch %o |> foo
endif
: |> touch %o |> bar
HERE
tup touch Tupfile
update

touch build/tup.config
update

check_exist build/bar
check_not_exist build/foo

tmkdir build-debug
echo "CONFIG_DEBUG=y" > build-debug/tup.config
tup touch build-debug/tup.config

update
check_exist build-debug/bar
check_exist build-debug/foo

# Make sure variant directories don't get propagated
tup_object_no_exist build build-debug
tup_object_no_exist build-debug build

eotup
