#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2022-2024  Mike Shal <marfey@gmail.com>
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

# Try to generate a shell script with a specific target as the goal.

. ./tup.sh

# 'tup generate' runs without a tup directory
rm -rf .tup

mkdir sub1
mkdir sub2
cat > Tuprules.tup << HERE
: foreach *.c |> ^ CC %f^ gcc -c %f -o %o |> %B.o
HERE
echo 'int foo;' > foo.c
echo 'int bar;' > sub1/bar.c
echo 'int bar2;' > sub1/bar2.c
echo 'int baz;' > sub2/baz.c
echo 'int baz2;' > sub2/baz2.c
echo 'include_rules' > Tupfile
cp Tupfile sub1
cp Tupfile sub2
echo ': *.o ../*.o ../sub2/*.o |> ar cr %o %f |> libfoo.a' >> sub1/Tupfile

generate --verbose $generate_script_name sub1/bar.o sub1/bar2.o
./$generate_script_name
check_exist sub1/bar.o sub1/bar2.o
check_not_exist sub2/baz.o sub2/baz2.o sub1/libfoo.a

eotup
