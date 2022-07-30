#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2022  Mike Shal <marfey@gmail.com>
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

# Later versions of valgrind have a pipe open that this test picks up and
# erroneously fails.
unset TUP_HELGRIND
unset TUP_VALGRIND

if [ ! "$tupos" = "Linux" ]; then
	echo "Sub-process fds only checked under linux. Skipping test."
	eotup
fi
cat > Tupfile << HERE
: |> ls -l /proc/self/fd > %o |> fds.txt
HERE
ls -l /proc/self/fd > .tup/curfds.txt
update

# On Gentoo, stdout points to output-0, while on Ubuntu, it points to the
# redirected file (fds.txt). This might be a bash vs dash thing.
cat fds.txt | grep -v ' 0 -> /dev/null' | grep -v ' 1 -> .*/output-' | grep -v ' 1 -> .*/fds.txt' | grep -v ' 2 -> .*/output' | grep -v ' 3 -> .*/deps-' | grep -v ' -> /proc/.*/fd' | while read i; do
	if ! grep -F "$i" .tup/curfds.txt > /dev/null; then
		echo "Error: $i shouldn't be open" 1>&2
		exit 1
	fi
done

eotup
