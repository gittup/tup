#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2015  Mike Shal <marfey@gmail.com>
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

# Apparently reading from the full path in a subdirectory is broken.

. ./tup.sh
# MinGW throws off the paths here, so doing 'cat /home/marf/tup/...' becomes
# 'cat C:\MinGW\msys\1.0\home\marf\tup\...' which doesn't exist
check_no_windows mingw
tmkdir atmp
cat > atmp/Tupfile << HERE
: |> cat $PWD/atmp/foo.txt |>
HERE
tup touch atmp/foo.txt atmp/Tupfile
update

eotup
