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

# Make sure we can move a directory that contains a file used by a Tupfile
# and have that Tupfile re-parse.

. ./tup.sh
check_monitor_supported

monitor
mkdir a
cat > a/Tupfile << HERE
include ../b/file
HERE
mkdir b
touch b/file
update

mv b c
update_fail_msg "Failed to parse included file.*b/file"

cat > a/Tupfile << HERE
include ../c/file
HERE
update

eotup
