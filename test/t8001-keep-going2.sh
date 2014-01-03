#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2014  Mike Shal <marfey@gmail.com>
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

# There was a problem where the tup entry list was being taken and not returned
# if the command had an error while processing its inputs (for example, if it
# read from a generated file and the dependency was missing). In this case, a
# second command that was successful would be unable to update its links. This
# test reproduces that by using the -k flag to keep going after the first error
# (it could also happen randomly when parallelized).

. ./tup.sh

cat > Tupfile << HERE
: |> echo '#define FOO 3' > %o |> foo.h
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE

cat > foo.c << HERE
#include "foo.h"
HERE

tup touch Tupfile foo.c zap.c
update_fail -k

if tup upd -k 2>&1 | grep gcc | wc -l | grep 1 > /dev/null; then
	:
else
	echo "Error: Only one file should have been compiled." 1>&2
	exit 1
fi

eotup
