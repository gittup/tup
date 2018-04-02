#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2018  Mike Shal <marfey@gmail.com>
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

# More TUP_CWD tests.

. ./tup.sh
check_no_windows paths # The path frobbing in node_exists() breaks this test
cat > Tupfile << HERE
include test1.tup
cflags += \$(TUP_CWD)
: |> echo \$(cflags) |>
HERE
cat > test1.tup << HERE
cflags += \$(TUP_CWD)
HERE
tup touch Tupfile test1.tup
parse
tup_object_exist . 'echo . .'

tmkdir foo
tmkdir foo/bar
mv test1.tup foo
tup rm test1.tup
tup touch foo/test1.tup

cat > Tupfile << HERE
include foo/test1.tup
cflags += \$(TUP_CWD)
: |> echo \$(cflags) |>
HERE
tup touch Tupfile
parse
tup_object_exist . 'echo foo .'

echo 'include bar/test2.tup' > foo/test1.tup
echo 'cflags += $(TUP_CWD)' > foo/bar/test2.tup
tup touch foo/test1.tup foo/bar/test2.tup
parse
tup_object_exist . 'echo foo/bar .'

rm Tupfile
tup rm Tupfile
cat > foo/Tupfile << HERE
include ../test1.tup
cflags += \$(TUP_CWD)
: |> echo \$(cflags) |>
HERE
echo 'cflags += $(TUP_CWD)' > test1.tup
tup touch foo/Tupfile test1.tup
parse
tup_object_exist foo 'echo .. .'

mv foo/Tupfile foo/bar/Tupfile
tup rm foo/Tupfile
tup touch foo/bar/Tupfile
echo 'include ../test2.tup' > foo/test1.tup
echo 'cflags += $(TUP_CWD)' > test2.tup
tup touch foo/test1.tup test2.tup
parse
tup_object_exist foo/bar 'echo ../.. .'

eotup
