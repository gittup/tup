#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2021-2023  Mike Shal <marfey@gmail.com>
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

# Try to use an output subdir with the same name as a variant.
. ./tup.sh

cat > Tupfile << HERE
: |> touch %o |> foo.txt
: |> touch %o |> build/bar.txt
HERE
update

check_exist foo.txt
check_exist build/bar.txt

rm -rf build
mkdir build
touch build/tup.config
update

check_exist build/foo.txt
check_exist build/build/bar.txt

eotup
