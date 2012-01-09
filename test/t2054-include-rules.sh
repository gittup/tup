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

# Test the include_rules directive.

. ./tup.sh
tmkdir fs
tmkdir fs/sub
cat > fs/Tupfile << HERE
include_rules
: foreach *.c |> gcc \$(CFLAGS) -c %f -o %o |> %B.o
: *.o |> gcc \$(LDFLAGS) %f -o %o |> prog
HERE
cp fs/Tupfile fs/sub/Tupfile

cat > Tuprules.tup << HERE
CFLAGS = -Wall
HERE
cat > fs/Tuprules.tup << HERE
CFLAGS += -DFS=1
LDFLAGS += -lfoo
HERE
cat > fs/sub/Tuprules.tup << HERE
CFLAGS = -O0
HERE

tup touch fs/Tupfile fs/sub/Tupfile Tuprules.tup fs/Tuprules.tup fs/sub/Tuprules.tup
tup touch fs/ext1.c fs/ext2.c
tup touch fs/sub/helper.c
tup parse

tup_dep_exist fs ext1.c fs 'gcc -Wall -DFS=1 -c ext1.c -o ext1.o'
tup_dep_exist fs ext2.c fs 'gcc -Wall -DFS=1 -c ext2.c -o ext2.o'
tup_dep_exist fs ext1.o fs 'gcc -lfoo ext1.o ext2.o -o prog'
tup_dep_exist fs ext2.o fs 'gcc -lfoo ext1.o ext2.o -o prog'

tup_dep_exist fs/sub helper.c fs/sub 'gcc -O0 -c helper.c -o helper.o'
tup_dep_exist fs/sub helper.o fs/sub 'gcc -lfoo helper.o -o prog'

tup_dep_exist . Tuprules.tup . fs
tup_dep_exist . Tuprules.tup fs sub
tup_dep_exist fs Tuprules.tup . fs
tup_dep_exist fs Tuprules.tup fs sub
tup_dep_no_exist fs/sub Tuprules.tup . fs
tup_dep_exist fs/sub Tuprules.tup fs sub

eotup
