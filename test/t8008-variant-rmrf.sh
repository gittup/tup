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

# Try rm -rf'ing a variant directory.
. ./tup.sh

tmkdir build
tmkdir build-debug

cat > Tupfile << HERE
ifeq (@(DEBUG),y)
: |> touch %o |> foo
endif
: |> touch %o |> bar
HERE
echo "CONFIG_DEBUG=y" > build-debug/tup.config
echo "" > build/tup.config
tup touch build/tup.config build-debug/tup.config Tupfile

update
check_exist build/bar
check_not_exist build/foo
check_exist build-debug/bar
check_exist build-debug/foo

rm -rf build-debug
update

tup_object_no_exist . build-debug

eotup
