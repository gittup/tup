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

# Apparently I broke Tuprules.tup with @-variables when I removed the
# tup_entry_add's a commit or two ago. Only happens when the monitor is
# running.

. ./tup.sh

check_monitor_supported
monitor
cat > Tuprules.tup << HERE
files-@(ARCH) = foo.c
HERE
cat > Tupfile << HERE
include_rules
HERE
varsetall ARCH=y
touch Tuprules.tup Tupfile
update

touch Tupfile
update

eotup
