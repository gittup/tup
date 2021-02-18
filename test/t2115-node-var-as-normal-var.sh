#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2021  Mike Shal <marfey@gmail.com>
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

# Test that trying to get the value of a node-variable using $() doesn't work.
# Since the $-variable doesn't exist, it just returns an empty string.

. ./tup.sh

cat > Tupfile << HERE
&node_var = lib.a
: |> echo \$(node_var) > %o |> out.txt
HERE

tup touch lib.a Tupfile
update

tup_dep_exist . 'echo  > out.txt' . out.txt

eotup
