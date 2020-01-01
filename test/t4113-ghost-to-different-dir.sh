#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2020  Mike Shal <marfey@gmail.com>
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

# Create a ghost directory, and then later try to make that a generated
# directory.

. ./tup.sh

cat > Tupfile << HERE
: foreach *.c | <headers> |> gcc -c %f -o %o -Ifoo -Ibar |> %B.o
HERE
echo '#include "sub/foo.h"' > ok.c

tmkdir bar
tmkdir bar/sub
tup touch bar/sub/foo.h
update

tup_dep_exist bar/sub foo.h . 'gcc -c ok.c -o ok.o -Ifoo -Ibar'

cat > bar/Tupfile << HERE
: |> touch %o |> ../foo/sub/foo.h | ../<headers>
HERE
tup touch bar/Tupfile
update

tup_dep_exist foo/sub foo.h . 'gcc -c ok.c -o ok.o -Ifoo -Ibar'
tup_dep_no_exist bar/sub foo.h . 'gcc -c ok.c -o ok.o -Ifoo -Ibar'

eotup
