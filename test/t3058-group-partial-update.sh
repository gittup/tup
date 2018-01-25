#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2018  Mike Shal <marfey@gmail.com>
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

# If we do a partial update, make sure we don't force compiling everything else
# in the group as well.

. ./tup.sh
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o | <group>
HERE
echo '#include "foo.h"' > foo.c
echo '#include "foo.h"' > bar.c
tup touch foo.c bar.c foo.h
update

tup touch foo.h
update_partial foo.o > .output.txt

if grep bar.o .output.txt > /dev/null; then
	cat .output.txt
	echo "Error: should not have compiled bar.o in partial update." 1>&2
	exit 1
fi

eotup
