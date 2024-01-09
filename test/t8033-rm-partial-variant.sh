#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2024  Mike Shal <marfey@gmail.com>
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

# Try removing part of a variant directory.
. ./tup.sh

mkdir build
mkdir sub

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o sub/*.o |> gcc %f -o %o |> prog.exe
HERE
cat > sub/Tupfile << HERE
: foreach bar.c |> gcc -c %f -o %o |> %B.o
HERE
echo "int main(void) {return 0;}" > foo.c
echo "CONFIG_FOO=y" > build/tup.config
touch sub/bar.c
update

check_exist build/foo.o build/sub/bar.o build/prog.exe
check_not_exist foo.o sub/bar.o prog.exe

rm -rf build/sub
update > .tupoutput 2>&1

if ! grep 'tup warning.*variant directory.*was deleted outside of tup' .tupoutput > /dev/null; then
	echo "Error: Expected to get a warning about deleting variant directories." 1>&2
	exit 1
fi

check_exist build/sub/bar.o

eotup
