#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2021-2022  Mike Shal <marfey@gmail.com>
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

# Try to use the 'import' keyword.

. ./tup.sh

export CC=gcc
mkdir sub1
mkdir sub2
cat > sub1/Tupfile << HERE
import CC
: foreach *.c |> \$(CC) -c %f -o %o |> %B.o
HERE
cat > sub2/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
touch sub1/foo.c sub2/bar.c
update

tup_dep_exist $ CC . sub1
tup_dep_no_exist $ CC . sub2

cat > sub1/marfcc << HERE
echo 'marf was here' > \$4
HERE

# Try to switch "compilers" and see if it gets updated.
export CC="sh marfcc"
update

echo 'marf was here' | diff - sub1/foo.o

# Drop the import and make sure the dependency goes away.
cat > sub1/Tupfile << HERE
: foreach *.c |> sh marfcc -c %f -o %o |> %B.o
HERE
update

tup_dep_no_exist $ CC . sub1

eotup
