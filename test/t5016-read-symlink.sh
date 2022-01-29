#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2022  Mike Shal <marfey@gmail.com>
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

# See if we can get a dependency on both the symlink and the file it points to.

. ./tup.sh
check_no_windows symlink

echo "#define FOO 3" > foo-x86.h
echo "#define FOO 4" > foo-ppc.h
ln -s foo-x86.h foo.h
echo '#include "foo.h"' > foo.c
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
update
check_exist foo.o

check_updates foo.h foo.o
check_updates foo-x86.h foo.o
check_no_updates foo-ppc.h foo.o

ln -sf foo-ppc.h foo.h
update
check_updates foo.h foo.o
check_no_updates foo-x86.h foo.o
check_updates foo-ppc.h foo.o

eotup
