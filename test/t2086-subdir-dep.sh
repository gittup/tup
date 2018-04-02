#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2018  Mike Shal <marfey@gmail.com>
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

# Make sure that when we remove a directory from a Tupfile, that directory
# no longer points to the Tupfile's directory.

. ./tup.sh
tmkdir subdir
cat > Tupfile << HERE
: subdir/*.c |> echo %f |>
HERE
tup touch Tupfile subdir/a.c
update
tup_dep_exist . subdir 0 .

rm Tupfile
tup rm Tupfile
update
tup_dep_no_exist . subdir 0 .

eotup
