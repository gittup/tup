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

. ./tup.sh
check_monitor_supported

# Apparently changing a Tupfile in between monitor invocations doesn't work
# properly (it doesn't get re-parsed).
monitor
cat > Tupfile << HERE
: |> echo hey |>
HERE
update
stop_monitor
tup_object_exist . 'echo hey'

cat > Tupfile << HERE
: |> echo yo |>
HERE
sleep 1
touch Tupfile
monitor
update
stop_monitor

tup_object_exist . 'echo yo'
tup_object_no_exist . 'echo hey'

eotup
