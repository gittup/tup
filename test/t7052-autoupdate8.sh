#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2012  Mike Shal <marfey@gmail.com>
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

# File creation events (such as the first time we run a command), should not
# result in the monitor re-running an autoupdate.

. ./tup.sh
check_monitor_supported
tup monitor --autoupdate > .monitor.output 2>&1
tup flush

cat > main.c << HERE
#include <stdio.h>
int main(void)
{
	return 0;
}
HERE
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> gcc %f -o %o |> prog
HERE
tup flush
check_exist prog

if ! cat .monitor.output | grep Updated | wc -l | grep 1 > /dev/null; then
	sleep 0.5
	if ! cat .monitor.output | grep Updated | wc -l | grep 1 > /dev/null; then
		echo "Monitor output:" 1>&2
		cat .monitor.output 1>&2
		echo "Error: tup should only update once" 1>&2
		exit 1
	fi
fi

eotup
