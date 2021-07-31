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

# Try to remove a config file that is the destination of a tup.config symlink.
# Note that a dangling tup.config symlink is still considered a valid (empty)
# config file.
. ./tup.sh
check_no_windows symlink

mkdir build-default
mkdir build-debug

mkdir configs

cat > Tupfile << HERE
ifeq (@(DEBUG),y)
: |> touch %o |> foo
endif
: |> touch %o |> bar
HERE
echo "CONFIG_DEBUG=y" > configs/debug.config
echo "" > configs/default.config
ln -s ../configs/debug.config build-debug/tup.config
ln -s ../configs/default.config build-default/tup.config

update

check_exist build-default/bar
check_not_exist build-default/foo
check_exist build-debug/bar
check_exist build-debug/foo

# Make sure variant directories don't get propagated
tup_object_no_exist build-default build-debug
tup_object_no_exist build-debug build-default

rm configs/debug.config
update

check_exist build-default/bar
check_not_exist build-default/foo
check_exist build-debug/bar
check_not_exist build-debug/foo

eotup
