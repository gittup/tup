#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2017  Mike Shal <marfey@gmail.com>
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

# Make sure if there is a ghost with the same name as a command, then we don't try to
# leave a ghost as a command pointing to an output.

. ./tup.sh
check_no_windows shell

cat > Tupfile << HERE
: |> if [ -f 'gcc -c foo.c' ]; then true; fi |>
HERE
tup touch Tupfile
update

cat > Tupfile << HERE
: |> if [ -f 'gcc -c foo.c' ]; then true; fi |>
: |> gcc -c foo.c |> foo.o
HERE
tup touch foo.c Tupfile
update_fail_msg "Unable to create command.*database as type 'ghost'"

eotup
