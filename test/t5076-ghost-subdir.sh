#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2018  Mike Shal <marfey@gmail.com>
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

# Verify gcc works if we add a header earlier in the include path.
. ./tup.sh

tmkdir a
tmkdir b
echo 'int x;' > b/foo.h
echo '#include "foo.h"' > ok.c

cat > Tupfile << HERE
: ok.c |> gcc -c %f -o %o -Ia -Ib |> ok.o
HERE
tup touch b/foo.h ok.c Tupfile
update

tup_dep_exist b foo.h . 'gcc -c ok.c -o ok.o -Ia -Ib'
sym_check ok.o x

echo 'int y;' > a/foo.h
update

tup_dep_exist a foo.h . 'gcc -c ok.c -o ok.o -Ia -Ib'
sym_check ok.o y

eotup
