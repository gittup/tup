#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2021  Mike Shal <marfey@gmail.com>
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

# When doing a readlink() on /proc/self in a chroot, we need to return the pid
# of the actual process doing the readlink(), not the fuse process.

. ./tup.sh
if [ ! "$tupos" = "Linux" ]; then
	echo "Only supported in Linux. Skipping test."
	eotup
fi
check_tup_suid
set_full_deps

cat > Tupfile << HERE
: |> ls -l /proc/self/ > /dev/null |>
HERE
update

eotup
