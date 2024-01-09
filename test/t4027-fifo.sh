#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2024  Mike Shal <marfey@gmail.com>
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

# Make sure fifos work when used as temporary files.

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

cat > ok.sh << HERE
#! /bin/sh
mkfifo fifotest

grep 'hey there' fifotest > /dev/null &
pid=\$!
echo "hey there" > fifotest
wait \$pid
status=\$?
rm fifotest
exit \$status
HERE

cat > Tupfile << HERE
: |> sh ok.sh |>
HERE
update

eotup
