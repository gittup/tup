#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2008-2018  Mike Shal <marfey@gmail.com>
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

. ./tup.sh
cp ../testTupfile.tup Tupfile

# Verify both files are compiled
(echo "#include \"foo.h\""; echo "int main(void) {return 0;}") > foo.c
(echo "#include \"foo.h\""; echo "void bar1(void) {}") > bar.c
echo "#define FOO 3" > foo.h
tup touch foo.c bar.c foo.h
update
sym_check foo.o main
sym_check bar.o bar1
sym_check prog.exe main bar1

# Rename bar.c to realbar.c.
mv bar.c realbar.c
tup rm bar.c
tup touch realbar.c
update
check_not_exist bar.o
tup_object_no_exist bar.c bar.o
sym_check realbar.o bar1
sym_check prog.exe main bar1

eotup
