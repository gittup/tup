#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2024  Mike Shal <marfey@gmail.com>
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

# Same as t7056, but now we modify a file in the generated dir before
# removing it.

. ./tup.sh
check_monitor_supported
monitor

cat > Tupfile << HERE
: |> touch %o |> foo/out.txt
HERE
update

check_exist foo/out.txt
check_not_exist bar

# Now switch to bar/ and make sure foo/ goes away.
cat > Tupfile << HERE
: |> touch %o |> bar/out.txt
HERE

# Touch the output file, so the monitor goes into that directory.
sleep 1
touch foo/out.txt
update

check_exist bar/out.txt
# foo/out.txt still exists because we overwrote it with the 'touch'
check_exist foo/out.txt

eotup
