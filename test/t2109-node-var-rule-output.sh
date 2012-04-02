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

# Test that using a node-variable as a rule output fails - this doesn't
# make any sense, since %-variables can only refer to existing files.

. ./tup.sh

cat > Tupfile << HERE
&toolkit_lib = tklib.a
: lib.a |> cp lib.a %o |> &(toolkit_lib)
HERE

tup touch lib.a tklib.a Tupfile

update_fail_msg "&-variables not allowed here"

eotup
