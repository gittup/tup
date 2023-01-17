#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2023  Mike Shal <marfey@gmail.com>
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

# Try tup --quiet with a command that fails due to dependency errors.

. ./tup.sh

set_quiet
cat > Tupfile << HERE
: |> touch bad.o |> foo.o
HERE
# Make sure we get the tup dependency error
update_fail_msg "tup error.*bad.*was written to"
# Make sure we get the banner for the command
update_fail_msg "touch bad.o"

eotup
