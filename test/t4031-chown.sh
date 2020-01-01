#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2020  Mike Shal <marfey@gmail.com>
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

# Try chown. Not sure how to really test this by changing the actual
# owner, or for running for people other than me.

. ./tup.sh
check_no_windows shell
check_no_ldpreload mozilla-unneeded
if ! whoami | grep marf > /dev/null; then
	echo "[33mSkip t4031 - you're not marf.[0m"
	eotup
fi

cat > Tupfile << HERE
: |> touch %o; chown marf %o |> test1
HERE
tup touch Tupfile
update

cat > Tupfile << HERE
: |> chown marf test2 |>
HERE
tup touch Tupfile test2
update_fail_msg "tup error.*chown"

eotup
