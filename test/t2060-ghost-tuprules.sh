#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2017  Mike Shal <marfey@gmail.com>
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

# Make sure include_rules makes ghost Tuprules.tup files.

. ./tup.sh
tmkdir fs
tmkdir fs/sub
cat > fs/sub/Tupfile << HERE
include_rules
: foreach *.c |> gcc \$(CFLAGS) -c %f -o %o |> %B.o
: *.o |> gcc \$(LDFLAGS) %f -o %o |> prog
HERE

cat > Tuprules.tup << HERE
CFLAGS = -Wall
LDFLAGS = -lm
HERE
cat > fs/sub/Tuprules.tup << HERE
CFLAGS += -O0
HERE

tup touch fs/sub/Tupfile Tuprules.tup fs/sub/Tuprules.tup
tup touch fs/sub/helper.c
parse

tup_object_exist fs/sub 'gcc -Wall -O0 -c helper.c -o helper.o'
tup_sticky_exist fs/sub helper.o fs/sub 'gcc -lm helper.o -o prog'

tup_dep_exist . Tuprules.tup fs sub
tup_dep_exist fs Tuprules.tup fs sub
tup_dep_exist fs/sub Tuprules.tup fs sub

cat > fs/Tuprules.tup << HERE
CFLAGS += -DFS=1
LDFLAGS += -lfoo
HERE
tup touch fs/Tuprules.tup
parse

tup_object_exist fs/sub 'gcc -Wall -DFS=1 -O0 -c helper.c -o helper.o'
tup_sticky_exist fs/sub helper.o fs/sub 'gcc -lm -lfoo helper.o -o prog'

eotup
