#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2015  Mike Shal <marfey@gmail.com>
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

# If a sub-process fails, the monitor sees that as a file deletion event
# and tries to remove the node. This shouldn't trigger the logic that
# determines if files have changed in handle_events, since the updater
# has already done all the necessary legwork. Here we check to make sure
# that the autoupdate runs only once if the command fails, and doesn't
# try to re-trigger itself (which would cause it to run a second time).

. ./tup.sh
check_monitor_supported
monitor --autoupdate > .monitor.output 2>&1

cat > ok.sh << HERE
#! /bin/sh
echo 'executed' >> .tup/.run.txt
touch foo
HERE

cat > Tupfile << HERE
: |> sh ok.sh && true |> foo
HERE
tup flush
check_exist foo

if ! cat .tup/.run.txt | wc -l | grep 1 > /dev/null; then
	echo "Error: tup should update only once" 1>&2
	exit 1
fi

cat > Tupfile << HERE
: |> sh ok.sh && false |> foo
HERE
tup flush

if ! cat .tup/.run.txt | wc -l | grep 2 > /dev/null; then
	echo "Error: tup should update only once" 1>&2
	exit 1
fi

eotup
