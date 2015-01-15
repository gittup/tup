#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2015  Mike Shal <marfey@gmail.com>
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

# Test that appending a normal string value to an existing node-variable
# doesn't work.

. ./tup.sh

cat > Tupfile << HERE
&node_var = lib.a
node_var += new value
: |> echo &(node_var) is \$(node_var) |>
HERE

tup touch lib.a Tupfile
update

tup_object_exist . 'echo lib.a is new value'

eotup
