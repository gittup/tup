#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2008-2012  Mike Shal <marfey@gmail.com>
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
cat > Tupfile << HERE
FOO := 1

srcs := bar.c
ifeq (1,\$(FOO))
srcs += foo.c
endif

: foreach \$(srcs) |> gcc -c %f -o %o |> %B.o
: *.o |> gcc %f -o %o |> prog.exe
HERE

echo "int main(void) {} void bar(void) {}" > bar.c
echo "void foo(void) {}" > foo.c
tup touch foo.c bar.c Tupfile
update
sym_check foo.o foo
sym_check bar.o bar main
sym_check prog.exe foo bar main

cat Tupfile | sed 's/FOO := 1/FOO := 0/' > tmpTupfile
mv tmpTupfile Tupfile
tup touch Tupfile
update

sym_check bar.o bar main
sym_check prog.exe bar main ^foo
check_not_exist foo.o

eotup
