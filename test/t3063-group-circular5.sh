#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2015  Mike Shal <marfey@gmail.com>
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

# Same as t3062, but with a new command instead of the same command.
. ./tup.sh

cat > Tupfile << HERE
: <foo> |> echo blah > %o |> output.txt | <bar>
HERE
update

cat > Tupfile << HERE
: <bar> |> echo moreinfo > %o |> newoutput.txt | <foo>
HERE
tup touch Tupfile
update

eotup
