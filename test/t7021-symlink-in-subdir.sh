#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2012  Mike Shal <marfey@gmail.com>
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

# While moving things around in the monitor, I found out that I don't have a
# test to check for a symlink working properly in a subdir. (By moving things
# around I found out I was relying on the cwd to be set for readlink() to work
# in update_symlink_file). This makes sure I don't break that again.
. ./tup.sh
check_monitor_supported

mkdir a
cd a
touch foo
ln -s foo bar
monitor
tup flush
tup_object_exist a foo
tup_object_exist a bar

eotup
