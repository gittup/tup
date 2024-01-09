#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013-2024  Mike Shal <marfey@gmail.com>
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

# Try to run an external script to get :-rules with a variant.
. ./tup.sh
check_no_windows run-script variant

mkdir build

cat > gen.sh << HERE
#! /bin/sh
for i in *.c; do
	pwd 1>&2
	echo ": \$i |> gcc -c %f -o %o |> %B.o" 1>&2
	echo ": \$i |> gcc -c %f -o %o |> %B.o"
done
HERE
chmod +x gen.sh
cat > Tupfile << HERE
: |> echo "" > %o |> gen.c
run ./gen.sh
HERE
touch foo.c bar.c build/tup.config
update

check_exist build/foo.o build/bar.o build/gen.o

# Now make sure we can just update the script and still get re-parsed.
cat > gen.sh << HERE
#! /bin/sh
HERE
update

check_not_exist build/foo.o build/bar.o build/gen.o
tup_dep_exist . gen.sh . build

# Now don't call gen.sh and make sure the dependency on the directory is gone.
cat > Tupfile << HERE
HERE
update

tup_dep_no_exist . gen.sh . build

eotup
