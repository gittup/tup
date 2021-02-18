#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2021  Mike Shal <marfey@gmail.com>
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

# Make sure we get a sane error message if we try to explicitly list a file
# in a ghost directory.
. ./tup.sh
check_no_windows variant

tmkdir build
tmkdir sub

cat > Tupfile << HERE
: |> if [ -f ghost-dir/foo ]; then cat ghost-dir/foo; else echo nofile; fi > %o |> output.txt
HERE
echo "CONFIG_FOO=y" > build/tup.config
tup touch build/tup.config Tupfile
update

echo 'nofile' | diff - build/output.txt
tup_object_exist . ghost-dir
tup_object_exist build ghost-dir

cat > sub/Tupfile << HERE
: ../ghost-dir/bar |> cat %f |>
HERE
parse_fail_msg "Explicitly named file 'bar' not found"

eotup
