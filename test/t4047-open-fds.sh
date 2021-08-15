#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2021  Mike Shal <marfey@gmail.com>
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

# Make sure a sub-process only has the minimum necessary fds open.
. ./tup.sh

if [ ! "$tupos" = "Linux" ]; then
	echo "Sub-process fds only checked under linux. Skipping test."
	eotup
fi
cat > Tupfile << HERE
: |> ls -l /proc/\$\$/fd > %o |> fds-while.txt
HERE

ls -l /proc/self/fd > .tup/fds-before.txt
update
ls -l /proc/self/fd > .tup/fds-after.txt

if [ "$(diff .tup/fds-before.txt .tup/fds-after.txt | grep -v ' 1 -> ' | grep -v ' 3 -> ' | grep -c '^>')" != 0 ]; then
  echo "tup has left open file descriptors"
  echo "File descriptors of parent process before running tup:"
  cat .tup/fds-before.txt
  echo "File descriptors of parent process while running tup:"
  cat fds-while.txt
  echo "File descriptors of parent process after running tup:"
  cat .tup/fds-after.txt
  exit 1
fi

eotup
