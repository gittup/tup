#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2021  Mike Shal <marfey@gmail.com>
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

# Try to run an external script to get :-rules in a sub-directory.
. ./tup.sh
check_no_windows run-script

mkdir sub
cat > sub/gen.sh << HERE
#! /bin/sh
for i in *.c; do
	echo ": \$i |> gcc -c %f -o %o |> %B.o"
done
HERE
chmod +x sub/gen.sh
cat > sub/Tupfile << HERE
run ./gen.sh
HERE
touch sub/foo.c sub/bar.c
update

check_exist sub/foo.o sub/bar.o

# Now make sure we can just update the script and still get re-parsed.
cat > sub/gen.sh << HERE
#! /bin/sh
HERE
update

check_not_exist sub/foo.o sub/bar.o
tup_dep_exist sub gen.sh . sub

# Now don't call gen.sh and make sure the dependency on the directory is gone.
cat > sub/Tupfile << HERE
HERE
update

tup_dep_no_exist sub gen.sh . sub

eotup
