#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2022  Mike Shal <marfey@gmail.com>
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

# For this bug, we have to create some command objects to be deleted. We need
# to successfully compile one file (bar.c). Then we change the commands, so the
# old ones are scheduled for deletion. The next update compiles bar.c again,
# but fails because foo.c is incorrect. This causes us to still have the old
# commands marked as deleted, but they haven't happened yet because we stopped
# on foo.c. Then foo.c is modified to be correct, and another update runs. Now
# the new (correct) command for bar.c is not in the partial DAG, but the old
# command to be deleted is. This causes bar.o to be incorrectly deleted on the
# third update
#
# I had to add a fake dependency from bar.o to the foo command because a random
# re-ordering of commands could make this test fail (ie: if foo happens to
# compile first).
. ./tup.sh
cat > Tupfile << HERE
: bar.c |> gcc -c bar.c -o bar.o |> bar.o
: foo.c bar.o |> gcc -c foo.c -o foo.o |> foo.o
: *.o |> gcc %f -o %o |> prog.exe
HERE

mkdir include
touch include/foo.h
(echo "#include \"foo.h\""; echo "void foo(void) {bork}") > foo.c
echo "int main(void) {}" > bar.c
update_fail
check_not_exist foo.o prog.exe
check_exist bar.o

tup_object_exist . 'gcc -c foo.c -o foo.o'
tup_object_exist . 'gcc -c bar.c -o bar.o'

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o -Iinclude |> %B.o
: *.o |> gcc %f -o %o |> prog.exe
HERE
update_fail
check_not_exist foo.o prog.exe

(echo "#include \"foo.h\""; echo "void foo(void) {}") > foo.c
update
sym_check foo.o foo
sym_check bar.o main
sym_check prog.exe main

tup_object_exist . 'gcc -c foo.c -o foo.o -Iinclude'
tup_object_exist . 'gcc -c bar.c -o bar.o -Iinclude'
tup_object_no_exist . 'gcc -c foo.c -o foo.o'
tup_object_no_exist . 'gcc -c bar.c -o bar.o'

eotup
