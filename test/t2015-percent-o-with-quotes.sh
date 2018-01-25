#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2018  Mike Shal <marfey@gmail.com>
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

# Stumbled on this while trying to build busybox.

. ./tup.sh
cat > Tupfile << HERE
: foo.c |> sh -c "gcc -c %f -o %o" |> %B.o
HERE
tup touch foo.c Tupfile
parse
tup_object_exist . foo.c
tup_object_exist . "sh -c \"gcc -c foo.c -o foo.o\""

eotup
