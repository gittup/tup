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

# Make sure we can have a command that creates a symlink get changed into a
# command that creates a regular file.

. ./tup.sh
check_no_windows symlink

cat > Tupfile << HERE
: |> ln -s foo bar |> bar
: bar |> cat %f > %o |> output
HERE
tup touch foo Tupfile
update

cat > Tupfile << HERE
: |> touch bar |> bar
: bar |> cat %f > %o |> output
HERE
tup touch Tupfile
update

# Make sure the sym field in bar no longer points to foo
check_updates bar output
check_no_updates foo output

eotup
