#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2024  Mike Shal <marfey@gmail.com>
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

# Use a variant with a generated directory.
. ./tup.sh
check_no_windows tup variant

touch tup.config
mkdir a
mkdir a/b
mkdir a/b/c

cat > a/b/c/Tupfile << HERE
: |> ^ Running test^ echo test > %o |> results/extra.txt
HERE
tup variant tup.config
update

cat > a/b/c/Tupfile << HERE
: |> false |>
HERE
update_fail_msg 'failed with return value 1'

check_not_exist build-tup/a/b/c/results/extra.txt
check_not_exist build-tup/a/b/c/results

cat > a/b/c/Tupfile << HERE
: |> ^ Running test^ echo test > %o |> results/extra.txt
HERE
update

check_exist build-tup/a/b/c/results/extra.txt

eotup
