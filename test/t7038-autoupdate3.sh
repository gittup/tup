#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2021  Mike Shal <marfey@gmail.com>
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

# Here we create a program so we have some generated files, then start up the
# monitor with autoupdate on. When the Tupfile is changed to no longer create
# the files, they are removed by the update. The monitor still has their
# entries cached, however, so when we try to add new generated files, the
# tupids get re-used, but the monitor can't insert them into the tree since it
# thinks they are already there.

. ./tup.sh
check_monitor_supported
set_autoupdate

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
tup flush
check_not_exist ok.o open.o prog

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> gcc %f -o %o |> prog
HERE
tup flush
check_exist prog

stop_monitor
eotup
