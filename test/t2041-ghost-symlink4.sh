#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2021  Mike Shal <marfey@gmail.com>
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

# Make a symlink to a ghost file in a ghost directory.
. ./tup.sh
check_no_windows symlink
ln -s spooky/ghost foo
cat > Tupfile << HERE
: |> cat foo 2>/dev/null || true |>
HERE
update
tup_object_exist . spooky foo

rm -f foo Tupfile
update
tup_object_no_exist . foo
tup_object_no_exist spooky ghost
tup_object_no_exist . spooky

eotup
