#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2024  Mike Shal <marfey@gmail.com>
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

# Similar bug to t7038 - when the files are removed by the update process, the
# tupids are still cached by the tup monitor process, so when the directory is
# created the monitor gets wonky and dies.

. ./tup.sh
check_monitor_supported

echo "int main(void) {return 0;}" > ok.c
touch open.c
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> gcc %f -o %o |> prog
HERE
update
check_exist prog

monitor

cat > Tupfile << HERE
HERE
update
check_not_exist ok.o open.o prog
mkdir foo
cp ok.c foo
cat > foo/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> gcc %f -o %o |> prog
HERE
update

check_exist foo/prog
stop_monitor

eotup
