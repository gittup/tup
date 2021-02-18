#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2021  Mike Shal <marfey@gmail.com>
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

# Simple test to verify that a client program can try to access variables,
# causing those variables to become ghosts.

. ./tup.sh
check_no_windows client

make_tup_client

cat > Tupfile << HERE
: |> ./client abcd |>
HERE
tup touch Tupfile
update

tup_object_exist tup.config abcd

cat > Tupfile << HERE
: |> ./client defg |>
HERE
tup touch Tupfile
update

tup_object_no_exist tup.config abcd
tup_object_exist tup.config defg

eotup
