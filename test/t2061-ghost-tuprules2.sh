#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2024  Mike Shal <marfey@gmail.com>
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

# Now make sure a ghost Tuprules.tup already exists before creating a new
# Tupfile to try to include it.

. ./tup.sh
mkdir fs
mkdir fs/sub
cat > fs/Tupfile << HERE
include_rules
: foreach *.c |> gcc \$(CFLAGS) -c %f -o %o |> %B.o
: *.o |> gcc \$(LDFLAGS) %f -o %o |> prog
HERE

touch fs/ok.c
touch fs/sub/helper.c
parse

tup_object_exist fs 'gcc  -c ok.c -o ok.o'

tup_dep_exist . Tuprules.tup . fs

cp fs/Tupfile fs/sub/Tupfile
parse

tup_object_exist fs/sub 'gcc  -c helper.c -o helper.o'

eotup
