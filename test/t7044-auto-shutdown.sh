#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2020  Mike Shal <marfey@gmail.com>
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

# Make sure if we rm the .tup/db file then the monitor shuts down.

. ./tup.sh
check_monitor_supported
monitor

rm .tup/db
x=0
max=5
if [ "$TUP_VALGRIND" = "1" ]; then
	max=500
fi
while ! grep "\-1" .tup/monitor.pid > /dev/null; do
	sleep 0.1
	x=$((x+1))
	if [ $x -gt $max ]; then
		echo "Error: monitor should have quit by now." 1>&2
		exit 1
	fi
done
wait_monitor

eotup
