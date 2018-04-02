#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2018  Mike Shal <marfey@gmail.com>
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

# Similar to t6062, but now we create the command first, then try to read from it as a file
# afterward.

. ./tup.sh
check_no_windows shell

cat > Tupfile << HERE
: |> gcc -c foo.c |> foo.o
HERE
tup touch Tupfile foo.c
update

cat > Tupfile << HERE
: |> if [ -f 'gcc -c foo.c' ]; then true; fi; touch out.txt |> out.txt
: |> gcc -c foo.c |> foo.o
HERE
tup touch Tupfile
update_fail_msg "tup error: Attempted to read from a file with the same name.*gcc -c foo.c"

eotup
