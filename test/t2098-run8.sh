#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2012  Mike Shal <marfey@gmail.com>
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

# Try a run-script with PATH
. ./tup.sh
check_no_windows run-script

export PATH=$PWD/a:$PATH

tmkdir a
cat > a/gen.sh << HERE
#! /bin/sh
for i in *.c; do
	echo ": \$i |> gcc -c %f -o %o |> %B.o"
done
HERE

tmkdir b
cat > b/gen.sh << HERE
#! /bin/sh
for i in *.c; do
	echo ": \$i |> gcc -Wall -c %f -o %o |> %B.o"
done
HERE
chmod +x a/gen.sh b/gen.sh
cat > Tupfile << HERE
HERE
tup touch Tupfile a/gen.sh b/gen.sh foo.c bar.c
update

# We should only get the directory-level dependency on PATH when we actually
# execute a run-script
tup_dep_no_exist $ PATH 0 .

cat > Tupfile << HERE
run gen.sh
HERE
tup touch Tupfile
update

tup_dep_exist $ PATH 0 .

check_exist foo.o bar.o
tup_object_exist . 'gcc -c foo.c -o foo.o'
tup_object_exist . 'gcc -c bar.c -o bar.o'

# Now just changing the PATH should cause the Tupfile to be re-parsed, resulting
# in the new run script to execute.
export PATH=$PWD/b:$PATH
update
tup_object_exist . 'gcc -Wall -c foo.c -o foo.o'
tup_object_exist . 'gcc -Wall -c bar.c -o bar.o'

eotup
