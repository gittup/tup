#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2015  Mike Shal <marfey@gmail.com>
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

# Test the %e extension variable, which is only valid in a foreach command.

. ./tup.sh
touch foo.c
touch bar.c
touch asm.S
cat > Tupfile << HERE
CFLAGS_S += -DASM
: foreach *.c *.S |> gcc \$(CFLAGS_%e) -c %f -o %o |> %B.o
HERE
tup touch foo.c bar.c asm.S Tupfile
update
tup_dep_exist . foo.c . 'gcc  -c foo.c -o foo.o'
tup_dep_exist . bar.c . 'gcc  -c bar.c -o bar.o'
tup_dep_exist . asm.S . 'gcc -DASM -c asm.S -o asm.o'

eotup
