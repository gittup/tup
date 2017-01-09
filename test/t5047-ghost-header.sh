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

# Make sure that if we remove a header so the compilation falls through to the
# next header, the first one becomes a ghost (the dependency should still be
# there).
. ./tup.sh

tmkdir foo
tmkdir bar
echo '#define FOO 3' > foo/me.h
echo '#define FOO 5' > bar/me.h
echo '#include "me.h"' > ok.c

cat > Tupfile << HERE
: ok.c |> gcc -c %f -o %o -Ifoo -Ibar |> ok.o
HERE
tup touch foo/me.h bar/me.h ok.c Tupfile
update

tup_dep_exist foo me.h . 'gcc -c ok.c -o ok.o -Ifoo -Ibar'
tup_dep_no_exist bar me.h . 'gcc -c ok.c -o ok.o -Ifoo -Ibar'

rm foo/me.h
tup rm foo/me.h
update

tup_dep_exist foo me.h . 'gcc -c ok.c -o ok.o -Ifoo -Ibar'
tup_dep_exist bar me.h . 'gcc -c ok.c -o ok.o -Ifoo -Ibar'

eotup
