#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2022  Mike Shal <marfey@gmail.com>
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

# Make sure we can have a fifo in the filesystem when scanning

. ./tup.sh
check_no_windows mkfifo
if [ "$tupos" = "Darwin" ]; then
	echo "[33mFIFOs currently unsupported in fuse4x. Skipping test.[0m"
	eotup
fi
if [ "$tupos" = "FreeBSD" ]; then
        echo "[33mFIFOs currently unsupported in FreeBSD. Skipping test.[0m"
        eotup
fi

mkfifo foofifo
cat > Tupfile << HERE
: |> echo hi |>
HERE
update

eotup
