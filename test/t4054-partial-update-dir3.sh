#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2017  Mike Shal <marfey@gmail.com>
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

# Make sure the partial update on a directory is recursive.

. ./tup.sh

tmkdir subdir
tmkdir subdir/extra
tmkdir notupdated

cat > subdir/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
cp subdir/Tupfile notupdated/Tupfile
cp subdir/Tupfile subdir/extra/Tupfile

echo 'void foo(void) {}' > subdir/foo.c
echo 'void bar(void) {}' > subdir/bar.c
echo 'void baz(void) {}' > notupdated/baz.c
echo 'void bar(void) {}' > subdir/extra/extra.c

update_partial subdir
check_exist subdir/foo.o
check_exist subdir/bar.o
check_exist subdir/extra/extra.o
check_not_exist notupdated/baz.o

update
check_exist subdir/foo.o
check_exist subdir/bar.o
check_exist subdir/extra/extra.o
check_exist notupdated/baz.o

eotup
