#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2021  Mike Shal <marfey@gmail.com>
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

. ./tup.sh
check_monitor_supported

# Make sure we don't get a warning for deleting a file if it was never created
# in the first place.
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
echo "int main(void) {return 0;}" > foo.c
tup scan
parse
if tup scan 2>&1 | grep 'tup warning: generated file.*was deleted outside of tup'; then
	echo "Received warning text from tup scan." 1>&2
	exit 1
fi

update
rm -f foo.o

if tup scan 2>&1 | grep 'tup warning: generated file.*was deleted outside of tup'; then
	echo "Above warning correctly received."
else
	echo "Did not receive warning from tup scan" 1>&2
	exit 1
fi

eotup
