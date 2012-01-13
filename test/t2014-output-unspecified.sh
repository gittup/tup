#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2012  Mike Shal <marfey@gmail.com>
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

# Verify that if a command writes to a file that it wasn't already linked to,
# it explodes. Things can't possibly work if the DAG is incomplete.

. ./tup.sh
cat > Tupfile << HERE
: foo.c |> gcc -c foo.c -o foo.o && touch bar |> foo.o
HERE
touch foo.c
tup touch foo.c Tupfile
update_fail_msg "File '.*bar' was written to"

eotup
