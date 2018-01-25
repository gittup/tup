#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2018  Mike Shal <marfey@gmail.com>
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

# Make sure wild-carding only matches the outputs from the current directory.
# We can't match files coming from other directories since we can't guarantee
# that the external Tupfiles will be parsed in time.

. ./tup.sh

tmkdir foo
tmkdir bar
cat > foo/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> ar cr %o %f |> libfoo.a
HERE
cat > bar/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> ../foo/%B.o
HERE
echo "int marfx;" > foo/x.c
echo "int marfy;" > foo/y.c
echo "int marfz;" > bar/z.c
tup touch foo/Tupfile bar/Tupfile
update

sym_check foo/libfoo.a marfx marfy ^marfz

tup touch foo/Tupfile
update

sym_check foo/libfoo.a marfx marfy ^marfz

eotup
