#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2023  Mike Shal <marfey@gmail.com>
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

# Like t5038, but in a subdirectory.
. ./tup.sh

mkdir sub
cat > sub/Tupfile << HERE
: |> touch foo; touch bar |> foo
HERE
update_fail

check_not_exist sub/bar

cat > sub/Tupfile << HERE
: |> touch foo |> foo
HERE
update

check_exist sub/foo
check_not_exist sub/bar

eotup
