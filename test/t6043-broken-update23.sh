#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2018  Mike Shal <marfey@gmail.com>
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

# If a command creates the correct output file but returns with a failure code,
# then that command is then removed before it ever executes successfully, the
# output file should be removed and not resurrected.
#
# See also t5066.
. ./tup.sh
check_no_windows shell

cat > Tupfile << HERE
: |> echo hey > ok.txt; exit 1 |> ok.txt
HERE
tup touch Tupfile
update_fail_msg "failed with return value 1"

cat > Tupfile << HERE
HERE
tup touch Tupfile
update
check_not_exist ok.txt

eotup
