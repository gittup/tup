#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2012  Mike Shal <marfey@gmail.com>
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

# Similar to t6046, but this now adds another command into the mix. The point
# of this test is to check for a longer chain in a circular dependency. In
# other words, the fix for t6046 can't simply do a 'foreach output tupid {remove
# link (tupid -> cmdid)}'.

. ./tup.sh
check_no_windows shell
cat > Tupfile << HERE
: |> cat foo 2>/dev/null || true; touch bar |> bar
: bar |> cat bar 2>/dev/null; touch foo |>
HERE
tup touch Tupfile
update_fail_msg "Unspecified output files"

check_not_exist foo

cat > Tupfile << HERE
: |> cat foo 2>/dev/null || true; touch bar |> bar
: bar |> cat bar 2>/dev/null; touch foo |> foo
HERE
tup touch Tupfile
update_fail_msg "Missing input dependency"

eotup
