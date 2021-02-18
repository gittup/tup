#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2020-2021  Mike Shal <marfey@gmail.com>
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

# Make sure including a lua file still allows the Tupfile to change variables.

. ./tup.sh

cat > extra.lua << HERE
CFLAGS += '-Dlua'
v1 = 17
v2 = {'hey', 'there'}
HERE

cat > Tupfile << HERE
CFLAGS += -Dfoo
include extra.lua
CFLAGS += -Dbar
: |> echo \$(CFLAGS) |>
: |> echo \$(v1) |>
: |> echo \$(v2) |>
HERE
parse

tup_object_exist . 'echo -Dfoo -Dlua -Dbar'
tup_object_exist . 'echo 17'
tup_object_exist . 'echo hey there'

eotup
