#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2024  Mike Shal <marfey@gmail.com>
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

# We shouldn't delete user files if a command tries to write them. The command
# will be writing to temporary files anyway, not actually overwriting the
# user file.

. ./tup.sh

echo "hey there" > foo.c
cat > Tupfile << HERE
: |> touch foo.c |>
HERE
update_fail_msg "\(tup error.*utimens\|Unspecified output files\)"

check_exist foo.c
echo "hey there" | diff - foo.c

eotup
