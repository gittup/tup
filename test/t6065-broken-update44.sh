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

# Make sure keep-going with a bunch of failed commands still runs successfully.
# A bug in the updater that was cleaning up failed nodes prematurely would
# cause a fuse internal error by screwing with freed memory.

. ./tup.sh

for i in `seq 1 10`; do touch $i.c; done

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o -include foo.h |> %B.o
HERE
if __update -k > .tupoutput.txt 2>&1; then
	cat .tupoutput.txt
	echo "Error: Expected update to fail" 1>&2
	exit 1
fi

if grep "fuse internal error" .tupoutput.txt >/dev/null; then
	cat .tupoutput.txt
	echo "Error: Detected fuse internal error" 1>&2
	exit 1
fi

eotup
