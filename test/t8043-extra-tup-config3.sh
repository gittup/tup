#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2021  Mike Shal <marfey@gmail.com>
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

# Try to create an extra tup.config in the root directory before a variant.
# Make sure it gets used after the variant is deleted.
. ./tup.sh

tmkdir build
tmkdir sub

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
echo "CONFIG_FOO=y" > tup.config
tup touch Tupfile foo.c sub/bar.c
update

check_not_exist build/prog.exe
check_exist prog.exe

# Now use a variant - note that we don't set @FOO so we don't get prog.exe in the
# variant.
touch build/tup.config
update

check_not_exist build/prog.exe
check_exist build/foo.o
check_not_exist prog.exe

# By disabling the variant, we should go back to using the default tup.config which
# should have @FOO set still.
rm build/tup.config
update

check_not_exist build/prog.exe
check_exist prog.exe

eotup
