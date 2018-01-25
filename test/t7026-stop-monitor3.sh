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

# Stop monitor test, now with .gitignore, which is handled kinda differently
# from other files.

. ./tup.sh
check_monitor_supported
monitor

mkdir .git
echo ".gitignore" > Tupfile
update
stop_monitor
tup_object_exist . .gitignore Tupfile

# If we just stop and then start the monitor after an update, no flags should
# be set.
monitor
check_empty_tupdirs
tup_object_exist . .gitignore Tupfile

eotup
