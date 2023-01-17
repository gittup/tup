#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2023  Mike Shal <marfey@gmail.com>
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

# See what happens if the command string doesn't change, but the input
# dependencies of the command do. The ghost node should still be removed.

. ./tup.sh
check_no_windows shell
cat > Tupfile << HERE
: ok.sh |> sh %f > %o |> output
HERE
cat > ok.sh << HERE
if [ -f ghost ]; then cat ghost; else echo nofile; fi
HERE
echo 'heyo' > foo.txt
update

tup_dep_exist . ghost . 'sh ok.sh > output'

echo 'nofile' | diff - output
cat > ok.sh << HERE
cat foo.txt
HERE
update

echo 'heyo' | diff - output
tup_object_no_exist . ghost

eotup
