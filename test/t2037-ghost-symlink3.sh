#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2024  Mike Shal <marfey@gmail.com>
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

# Make sure that if we have two symlinks pointing to the same ghost, deleting
# one symlink doesn't kill the ghost.

. ./tup.sh
check_no_windows symlink
ln -s ghost foo
ln -s ghost bar
cat > Tupfile << HERE
: foreach foo bar |> cat %f 2>/dev/null || true |>
HERE
update
tup_object_exist . ghost foo bar

rm -f foo
cat > Tupfile << HERE
: bar |> cat %f 2>/dev/null || true |>
HERE
update
tup_object_no_exist . foo
tup_object_exist . ghost bar

rm -f bar Tupfile
update
tup_object_no_exist . ghost foo bar

eotup
