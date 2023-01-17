#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2014-2023  Mike Shal <marfey@gmail.com>
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

# Ensure that rmdir errors against a tmpdir that is not empty

. ./tup.sh
check_no_windows shell

check_tup_suid

expected="rmdir.*/tmp/foo.*Directory not empty"

# contains subdirectory
cat > Tupfile << HERE
: |> mkdir -p /tmp/foo/bar && rmdir /tmp/foo |>
HERE

update_fail_msg "$expected"

# contains file
cat > Tupfile << HERE
: |> mkdir -p /tmp/foo && touch /tmp/foo/bar && rmdir /tmp/foo |>
HERE

update_fail_msg "$expected"

eotup
