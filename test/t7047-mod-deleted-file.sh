#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2023  Mike Shal <marfey@gmail.com>
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

# If we get an event that a file was modified, but the file was removed or
# renamed before we can process the event, the monitor should just
# delete the node from the db instead of failing an fstatat() call.
. ./tup.sh
check_monitor_supported

monitor

num=100
for i in `seq 1 $num`; do
	touch foo$i
	echo "hey" > foo$i
done

# Start processing all the create/modify events we made above, while simultaneously removing
# all the files. Some of these will be gone when we process the modify events, but we
# won't have gotten the delete events that would remove the modify events from the queue.
tup flush & rm foo*

stop_monitor

eotup
