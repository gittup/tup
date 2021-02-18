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

# We have a ghost node, and then move a directory over that node. Since the
# directory node just gets renamed, we have to make sure the ghost becomes a
# normal node.
. ./tup.sh
check_monitor_supported
monitor
mkdir a
mkdir a/a2
echo 'heyo' > a/ghost
echo ': |> if [ -f b/ghost ]; then cat b/ghost; else echo nofile; fi > %o |> output' > Tupfile

update
echo 'nofile' | diff - output

mv a b
update
echo 'heyo' | diff - output

eotup
