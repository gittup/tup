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

# We have a ghost node that is pointed to by another node, then move a
# directory over the parent ghost. The rule should execute.
. ./tup.sh
check_monitor_supported
monitor

ln -s secret/ghost a
cat > Tupfile << HERE
: |> (cat a 2>/dev/null || echo nofile) > %o |> output.txt
HERE
update
echo nofile | diff - output.txt

mkdir foo
echo hey > foo/ghost
update
echo nofile | diff - output.txt

mv foo secret
update
echo hey | diff - output.txt

eotup
