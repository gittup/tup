#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2021  Mike Shal <marfey@gmail.com>
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

# We don't actually need a dependency on the directory a group is in, since we
# aren't reading file entries from it.

. ./tup.sh
mkdir sub
mkdir bar
cat > sub/Tupfile << HERE
: ../bar/<group> |> echo foo |>
HERE
touch bar/Tupfile
update

tup_dep_no_exist . bar . sub

cat > sub/Tupfile << HERE
: ../bar/<group> ../bar/*.c |> echo foo |>
HERE
update

tup_dep_exist . bar . sub

eotup
