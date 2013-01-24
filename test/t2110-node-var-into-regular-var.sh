#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2012  Mike Shal <marfey@gmail.com>
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

# Test that using a node-variable as the value of a regular variable doesn't
# work - converting a node-variable to a string only makes sense in places
# where the string will be consumed immediately. Putting the string into a
# variable is not one of these places (the variable could be used as an output
# file, or it could be used in another Tupfile where the relative path is no
# longer valid).

. ./tup.sh

cat > Tupfile << HERE
node_var = tup.nodevariable 'lib.a'
var = tostring(node_var)
HERE

tup touch lib.a Tupfile

update_fail_msg "&-variables not allowed here"

eotup
