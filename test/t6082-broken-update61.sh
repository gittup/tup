#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2020  Mike Shal <marfey@gmail.com>
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

# Follow-on to t6081 - make sure we do all the usual ghost cleanup handling
# after deleting the node if it is still a ghost.

. ./tup.sh
check_no_windows shell
cat > Tupfile << HERE
: |> cat sub/foo 2>/dev/null || true; touch bar |> bar
: bar |> cat bar 2>/dev/null; touch sub |>
HERE
tup touch sub
tup touch Tupfile
update_fail_msg "Unspecified output files"

cat > Tupfile << HERE
: |> cat sub/foo 2>/dev/null || true; touch bar |> bar
: bar |> cat bar 2>/dev/null; touch sub |> sub
HERE
rm -rf sub
mkdir sub
touch sub/unused
tup touch Tupfile
update_fail_msg "Attempting to insert 'sub' as a generated node"

eotup
