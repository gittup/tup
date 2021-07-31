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

# Include a separate Tupfile, then stop including it and make sure the link to
# the directory goes away. I put everything in a tmp directory because
# tup_dep_exist doesn't work using '. .' (since I need to use 0 as the parent
# for '.' and I don't feel like fixing it...and I realized all these ''s and
# .'s look like faces.)
#
# Actually I fixed it so you can use the string "0" to represent the parent of
# the '.' directory (I figure since the tup_dep_exist thing is only used by
# test-cases, so I just won't generate a file called "0" and it will be fine -
# it doesn't impact users with 0 files), but I'm leaving this test the way it
# is because the faces are silly.

. ./tup.sh
mkdir tmp
cat > tmp/Tupfile << HERE
include Tupfile.vars
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> gcc -o %o %f |> prog.exe
HERE

cat > tmp/Tupfile.vars << HERE
VAR = yo
HERE

echo "int main(void) {return 0;}" > tmp/foo.c
touch tmp/bar.c
update
tup_object_exist tmp foo.c bar.c
tup_object_exist tmp "gcc -c foo.c -o foo.o"
tup_object_exist tmp "gcc -c bar.c -o bar.o"
tup_object_exist tmp "gcc -o prog.exe bar.o foo.o"
tup_dep_exist tmp Tupfile.vars . tmp

cat > tmp/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> gcc -o %o %f |> prog.exe
HERE
update
tup_dep_no_exist tmp Tupfile.vars . tmp

eotup
