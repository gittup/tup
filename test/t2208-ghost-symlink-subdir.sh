#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2018  Mike Shal <marfey@gmail.com>
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

# Same as t2034, but in a subdir.

. ./tup.sh
check_no_windows symlink
tmkdir sub
ln -s ghost sub/foo
cat > sub/Tupfile << HERE
: |> cat foo 2> /dev/null || true |>
HERE
tup touch sub/foo sub/Tupfile
update
tup_object_exist sub ghost foo

rm -f sub/foo sub/Tupfile
tup rm sub/foo sub/Tupfile
update
tup_object_no_exist sub ghost foo

eotup
