#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2018  Mike Shal <marfey@gmail.com>
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

# Try to run an external script to get :-rules.
. ./tup.sh
check_no_windows run-script

cat > gen.sh << HERE
#! /bin/sh
for i in *.c; do
	echo ": \$i |> gcc -c %f -o %o |> %B.o"
done
HERE
chmod +x gen.sh
cat > Tupfile << HERE
run ./gen.sh
HERE
tup touch Tupfile gen.sh foo.c bar.c
update

check_exist foo.o bar.o

# Now make sure we can just update the script and still get re-parsed.
cat > gen.sh << HERE
#! /bin/sh
HERE
tup touch gen.sh
update

check_not_exist foo.o bar.o
tup_dep_exist . gen.sh 0 .

# Now don't call gen.sh and make sure the dependency on the directory is gone.
cat > Tupfile << HERE
HERE
tup touch Tupfile
update

tup_dep_no_exist . gen.sh 0 .

eotup
