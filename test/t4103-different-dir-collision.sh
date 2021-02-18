#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2021  Mike Shal <marfey@gmail.com>
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

# Make sure we can handle cases where two directories try to write to the
# same output file in a different directory.

. ./tup.sh

tmkdir foo
tmkdir bar
tmkdir baz
echo foo > foo/foo.txt
echo bar > bar/bar.txt
cat > foo/Tupfile << HERE
: |> cp foo.txt %o |> ../baz/baz.txt
HERE
cat > bar/Tupfile << HERE
: |> cp bar.txt %o |> ../baz/baz.txt
HERE
tup touch foo/Tupfile bar/Tupfile
update_fail_msg "Unable to create output file '../baz.baz.txt'"

# Correctly create foo.txt
cat > bar/Tupfile << HERE
HERE
tup touch bar/Tupfile
update

echo foo | diff - baz/baz.txt

# Switch from foo to bar - this should work.
cat > foo/Tupfile << HERE
HERE
cat > bar/Tupfile << HERE
: |> cp bar.txt %o |> ../baz/baz.txt
HERE
tup touch foo/Tupfile bar/Tupfile
update

echo bar | diff - baz/baz.txt

# Switch from bar to foo - this should work.
cat > foo/Tupfile << HERE
: |> cp foo.txt %o |> ../baz/baz.txt
HERE
cat > bar/Tupfile << HERE
HERE
tup touch foo/Tupfile bar/Tupfile
update

echo foo | diff - baz/baz.txt

# Back to both - should get a failure.
cat > bar/Tupfile << HERE
: |> cp bar.txt %o |> ../baz/baz.txt
HERE
tup touch bar/Tupfile
update_fail_msg "Unable to create output file '../baz.baz.txt'"

eotup
