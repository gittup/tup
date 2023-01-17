#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2023  Mike Shal <marfey@gmail.com>
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

# Make sure we can go from a variant to in-tree build with a tup.config
. ./tup.sh

mkdir build
mkdir sub

cat > Tupfile << HERE
.gitignore
: foreach *.c |> gcc -c %f -o %o |> %B.o
ifeq (@(FOO),y)
: *.o sub/*.o |> gcc %f -o %o |> prog.exe
endif
HERE
cat > sub/Tupfile << HERE
.gitignore
: foreach bar.c |> gcc -c %f -o %o |> %B.o
HERE
echo "int main(void) {return 0;}" > foo.c
echo 'CONFIG_FOO=y' > build/tup.config
touch sub/bar.c
update

check_exist build/foo.o build/sub/bar.o build/prog.exe
check_not_exist foo.o sub/bar.o prog.exe

mv build/tup.config .
update

check_not_exist build/foo.o build/sub/bar.o build/prog.exe build/sub
check_exist foo.o sub/bar.o prog.exe

rm tup.config
update

check_not_exist prog.exe

eotup
