#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2023  Mike Shal <marfey@gmail.com>
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

# Make sure we aren't trying to unlink() command nodes. We can verify this
# by creating a very long command string, so that if we do try to unlink
# it, it will fail with a name too long error.
. ./tup.sh
check_no_windows shell

mkdir build

for i in `seq 1 100`; do
	text="$text $i"
done

cat > Tupfile << HERE
.gitignore
: |> ^ echo^ echo $text > /dev/null |>
HERE
touch build/tup.config
update

rm build/tup.config
update

eotup
