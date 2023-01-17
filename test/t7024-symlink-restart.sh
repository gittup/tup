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

# Make sure a symlink doesn't do anything funky when the monitor is restarted
. ./tup.sh
check_monitor_supported
monitor

# First set everything up and clear all flags
mkdir foo-x86
ln -s foo-x86 foo
update

# Now fake the mtime to be old, and update again to clear the flags (since the
# mtime is different, it will get put into modify)
stop_monitor
tup fake_mtime foo 5
monitor
stop_monitor
update

# Now the mtime should match the symlink. Start and stop the monitor again to
# see that no flags are set.
monitor
stop_monitor
check_empty_tupdirs

eotup
