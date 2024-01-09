#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2015-2024  Mike Shal <marfey@gmail.com>
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

# Add, remove, and add again an environment variable to a command.

. ./tup.sh

cat > Tupfile << HERE
export FOO
: |> touch %o |> out.txt
HERE
update

tup_dep_exist $ FOO . 'touch out.txt'

cat > Tupfile << HERE
: |> touch %o |> out.txt
HERE
parse

cat > Tupfile << HERE
export FOO
: |> touch %o |> out.txt
HERE
update

tup_dep_exist $ FOO . 'touch out.txt'

eotup
