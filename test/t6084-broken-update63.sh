#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2020  Mike Shal <marfey@gmail.com>
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

# Make sure we can parse a Tupfile that writes a file outside of the hierarchy
# without crashing.

. ./tup.sh
root=$PWD
mkdir sub
cd sub
re_init
cat > Tupfile << HERE
: |> touch %o |> $root/external.txt
HERE
update_fail_msg "Unable to write to a file outside of the tup hierarchy.*external.txt"

eotup
