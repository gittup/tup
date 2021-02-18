#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2021  Mike Shal <marfey@gmail.com>
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

# Same as t5017, only the symlink points to a directory instead of a file.

. ./tup.sh
check_no_windows symlink
check_no_ldpreload symlink-dir
mkdir arch-x86
mkdir arch-ppc
tup touch arch-x86 arch-ppc
echo "#define FOO 3" > arch-x86/foo.h
echo "#define FOO 4" > arch-ppc/foo.h
ln -s arch-x86 arch
echo '#include "arch/foo.h"' > foo.c
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
tup touch Tupfile foo.c arch-x86/foo.h arch-ppc/foo.h arch
update
check_exist foo.o

check_updates arch-x86/foo.h foo.o
check_no_updates arch-ppc/foo.h foo.o

rm -f arch
ln -sf arch-ppc arch
tup touch arch
update
check_no_updates arch-x86/foo.h foo.o
check_updates arch-ppc/foo.h foo.o

eotup
