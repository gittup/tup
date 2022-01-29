#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2022  Mike Shal <marfey@gmail.com>
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

# Make sure if a group is updated we don't force updating everything that uses
# the group.

. ./tup.sh
single_threaded
cat > Tupfile << HERE
: foreach *.h.in |> cp %f %o |> %B <foo-autoh>
: foreach *.c | <foo-autoh> |> gcc -c %f -o %o |> %B.o {objs}
HERE
echo '#define FOO 3' > foo.h.in
cat > foo.c << HERE
#include "foo.h"
HERE
touch bar.c
update

touch foo.h.in
if [ "$(tup | grep -c 'gcc -c')" != 1 ]; then
	echo "Error: Expected only one file to compile" 1>&2
	exit 1
fi

touch bar.h.in
if [ "$(tup | grep -c 'gcc -c')" != 0 ]; then
	echo "Error: Expected no files to compile" 1>&2
	exit 1
fi

eotup
