#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2024  Mike Shal <marfey@gmail.com>
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
check_monitor_supported

# Verify that 'tup scan' works as a one-shot monitor. Now 'tup scan' is called
# automatically by 'tup upd' when necessary.
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> gcc %f -o %o |> prog
HERE
echo "int main(void) {return 0;}" > foo.c
tup upd
check_exist foo.o
check_exist prog

# Add new file
echo "void bar(void) {}" > bar.c
tup upd
check_exist bar.o
sym_check prog bar

# Modify file
echo "void bar2(void) {}" >> bar.c
sleep 1
touch bar.c
tup upd
sym_check prog bar bar2

# Delete file
rm bar.c
tup upd
sym_check prog ^bar ^bar2

# Modify Tupfile
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
touch -t 202005080000 Tupfile
tup upd
check_not_exist prog

eotup
