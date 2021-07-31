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

# Go from in-tree to variant to in-tree to variant with a tup.config for
# the in-tree build.
. ./tup.sh

mkdir build

cat > Tupfile << HERE
.gitignore
ifeq (@(DEBUG),y)
: |> touch %o |> foo
endif
: |> touch %o |> bar
HERE
echo 'CONFIG_DEBUG=y' > tup.config
update

check_exist foo bar
check_not_exist build/foo build/bar

touch build/tup.config
update

check_not_exist foo bar
check_not_exist build/foo
check_exist build/bar

rm build/tup.config
update

check_exist foo bar
check_not_exist build/foo build/bar

touch build/tup.config
update

check_not_exist foo bar
check_not_exist build/foo
check_exist build/bar

eotup
