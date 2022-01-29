#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2021-2022  Mike Shal <marfey@gmail.com>
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

# Try to import with a default value if unset

. ./tup.sh

unset MARFNOEXIST
cat > Tupfile << HERE
import MARFNOEXIST=defaultval
: |> Var is: \$(MARFNOEXIST) end |>
HERE
parse

tup_dep_exist $ MARFNOEXIST 0 .
tup_object_exist . 'Var is: defaultval end'

export MARFNOEXIST=nowexists
parse

tup_object_exist . 'Var is: nowexists end'

eotup
