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

# Make sure autoupdate works when the monitor has fchdir'd down a few
# directories.

. ./tup.sh
check_monitor_supported
set_autoupdate
monitor
cat > foo.c << HERE
int main(void) {return 3;}
HERE
cat > Tupfile << HERE
: foo.c |> gcc %f -o %o |> prog
HERE

tup flush
check_exist prog

mkdir foo
cp foo.c foo
cp Tupfile foo

tup flush
check_exist foo/prog

mkdir foo/bar
cp foo.c foo/bar
cp Tupfile foo/bar

tup flush
check_exist foo/bar/prog

eotup
