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

# Try to remove both the variant subdirectory and the srctree subdirectory at
# the same time.
. ./tup.sh
check_no_windows variant

tmkdir build
tmkdir sub

# Add a bunch of random directories. This is to try to trigger us removing a
# sub/X directory before the build/sub/X directory. The removal order is based
# on how the nodes end up in an rb tree, so while it is consistent it is not
# necessarily predictable.
tmkdir sub/1
tmkdir sub/2
tmkdir sub/3
tmkdir sub/4
tmkdir sub/5
tmkdir sub/6

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> gcc %f -o %o |> prog
HERE
cat > sub/Tupfile << HERE
: foreach bar.c |> gcc -c %f -o %o |> %B.o
HERE
echo "int main(void) {return 0;}" > foo.c
echo "CONFIG_FOO=y" > build/tup.config
tup touch build/tup.config Tupfile foo.c sub/bar.c
update

tmkdir sub/dir2
tmkdir sub/dir2/dir3
cat > sub/dir2/Tupfile << HERE
: foreach bar2.c |> gcc -c %f -o %o |> %B.o
HERE
cat > sub/dir2/dir3/Tupfile << HERE
: foreach bar3.c |> gcc -c %f -o %o |> %B.o
HERE
tup touch sub/dir2/bar2.c sub/dir2/dir3/bar3.c
update

check_exist build/foo.o build/sub/bar.o build/sub/dir2/bar2.o build/sub/dir2/dir3/bar3.o build/prog
check_not_exist foo.o sub/bar.o prog

rm -rf build/sub sub
update

eotup
