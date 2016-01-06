#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2016  Mike Shal <marfey@gmail.com>
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

# This broke in commit d404764edf235a8e7aaa9d4cfdb1055bcd791aa6. I was using
# a per-Tupfile delete tree, which doesn't work when a dependent subdirectory
# uses files that may be deleted. It worked in the earlier delete_tree removal
# because I was deleting files immediately. I still like deleting files in a
# separate "phase", but there needs to be a global cache of which files that
# will be.
#
# This test just mimics how it was discovered.

. ./tup.sh
tmkdir sub
cat > sub/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
cat > Tupfile << HERE
: sub/*.o |> ar rcs %o %f |> libfoo.a
HERE
tup touch sub/foo.c sub/bar.c sub/Tupfile Tupfile
update

rm sub/bar.c
tup rm sub/bar.c
update

eotup
