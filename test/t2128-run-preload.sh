#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2024  Mike Shal <marfey@gmail.com>
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

# Like t2095, but use the preload keyword to load the subdirectory contents.
. ./tup.sh
check_no_windows run-script

mkdir sub
cat > sub/Tupfile << HERE
preload sub2 ../foo/bar
run sh -e ok.sh
HERE
cat > sub/ok.sh << HERE
for i in *.c sub2/*.[co] ../foo/bar/*.[co]; do
	echo ": |> echo \$i |>"
done
HERE
touch sub/sub.c

mkdir sub/sub2
cat > sub/sub2/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
touch sub/sub2/foo.c sub/sub2/bar.c

mkdir foo
mkdir foo/bar
cat > foo/bar/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
touch foo/bar/ok.c
update

tup_object_exist sub 'echo sub.c'
tup_object_exist sub 'echo sub2/foo.c'
tup_object_exist sub 'echo sub2/bar.c'
tup_object_exist sub 'echo sub2/foo.o'
tup_object_exist sub 'echo sub2/bar.o'
tup_object_exist sub 'echo ../foo/bar/ok.c'
tup_object_exist sub 'echo ../foo/bar/ok.o'

eotup
