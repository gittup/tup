#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2014  Mike Shal <marfey@gmail.com>
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

# Same as t5016, only the symlink is generated from a rule.

. ./tup.sh
check_no_windows symlink
echo "#define FOO 3" > foo-x86.h
echo "#define FOO 4" > foo-ppc.h
echo '#include "foo.h"' > foo.c
cat > Tupfile << HERE
ARCH = x86
: foo-\$(ARCH).h |> ln -s %f %o |> foo.h
: foreach *.c | foo.h |> gcc -c %f -o %o |> %B.o
HERE
tup touch foo.c foo-x86.h foo-ppc.h
update
check_exist foo.o

check_updates foo-x86.h foo.o
check_updates foo.h foo.o
check_no_updates foo-ppc.h foo.o

cat > Tupfile << HERE
ARCH = ppc
: foo-\$(ARCH).h |> ln -s %f %o |> foo.h
: foreach *.c | foo.h |> gcc -c %f -o %o |> %B.o
HERE
tup touch Tupfile
update
check_updates foo-ppc.h foo.o
check_updates foo.h foo.o
check_no_updates foo-x86.h foo.o

eotup
