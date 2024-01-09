#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2008-2024  Mike Shal <marfey@gmail.com>
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

# Test that a rule in a Tupfile can use variables

. ./tup.sh
cat > Tupfile << HERE
CC = gcc
CFILES = *.c
OFILES = *.o
EXE = prog.exe

: foreach \$(CFILES) |> \$(CC) -c %f -o %o |> %B.o
: \$(OFILES) |> \$(CC) %f -o \$(EXE) |> \$(EXE)
HERE

echo "int main(void) {} void foo(void) {}" > foo.c
update
sym_check foo.o foo
sym_check prog.exe foo
tup_object_exist . "gcc foo.o -o prog.exe"
tup_object_exist . "gcc -c foo.c -o foo.o"
tup_object_exist . prog.exe foo.o

eotup
