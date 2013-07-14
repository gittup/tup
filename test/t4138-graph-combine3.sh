#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2013  Mike Shal <marfey@gmail.com>
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

# Make sure we don't duplicate links when commands are combined.
. ./tup.sh

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> gcc %f -o %o |> prog.exe
HERE
tup touch foo.c bar.c baz.c foo.h
echo "int main(void) {return 0;}" > foo.c
echo "#include \"foo.h\"" > bar.c
echo "#include \"foo.h\"" > baz.c
update

tup graph . --combine > ok.dot
gitignore_good 'node.*\.o.*3 files' ok.dot
gitignore_good 'node.*\.c.*4 files' ok.dot
gitignore_good 'node.*gcc -c.*3 commands' ok.dot

eotup
