#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2018  Mike Shal <marfey@gmail.com>
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

# Make sure that updating a directory with no generated files doesn't do
# anything.

. ./tup.sh

tmkdir subdir
tmkdir notupdated
tmkdir docs

cat > subdir/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
cp subdir/Tupfile notupdated/Tupfile

echo 'void foo(void) {}' > subdir/foo.c
echo 'void bar(void) {}' > subdir/bar.c
echo 'void baz(void) {}' > notupdated/baz.c
update_partial docs

check_not_exist subdir/foo.o
check_not_exist subdir/bar.o
check_not_exist notupdated/baz.o

update_partial subdir
check_exist subdir/foo.o
check_exist subdir/bar.o
check_not_exist notupdated/baz.o

update
check_exist subdir/foo.o
check_exist subdir/bar.o
check_exist notupdated/baz.o

eotup
