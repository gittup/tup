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

# Same as t5016, only the link points to a file in a subdirectory.

. ./tup.sh
tmkdir arch
echo "#define FOO 3" > arch/foo-x86.h
echo "#define FOO 4" > arch/foo-ppc.h
ln -s arch/foo-x86.h foo.h
echo '#include "foo.h"' > foo.c
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
tup touch foo.c arch/foo-x86.h arch/foo-ppc.h foo.h
update
check_exist foo.o

check_updates foo.h foo.o
check_updates arch/foo-x86.h foo.o
check_no_updates arch/foo-ppc.h foo.o

ln -sf arch/foo-ppc.h foo.h
tup touch foo.h
update
check_updates foo.h foo.o
check_no_updates arch/foo-x86.h foo.o
check_updates arch/foo-ppc.h foo.o

eotup
