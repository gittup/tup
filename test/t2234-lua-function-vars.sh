#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2022-2024  Mike Shal <marfey@gmail.com>
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

# Make sure we don't get extra lua variables for things like _G or rawget or
# other functions. This would happen if we include a lua file in another lua fil

. ./tup.sh
cat > Tupfile << HERE
include ok.lua
: |> echo "Vars are: \$(_G) \$(rawget) \$(tup) \$(CFLAGS)" |>
HERE
cat > rules.lua << HERE
CFLAGS += '-DBAZ'
HERE
cat > ok.lua << HERE
tup.include('rules.lua')
HERE
update

tup_object_exist . 'echo "Vars are:    -DBAZ"'

eotup
