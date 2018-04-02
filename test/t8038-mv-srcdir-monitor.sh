#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2018  Mike Shal <marfey@gmail.com>
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

# Try moving a src directory with a variant build using the monitor.
. ./tup.sh
check_no_windows variant
check_monitor_supported

monitor

mkdir build
mkdir sub

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o sub/*.o |> gcc %f -o %o |> prog
HERE
cat > sub/Tupfile << HERE
: foreach bar.c |> gcc -c %f -o %o |> %B.o
HERE
echo "int main(void) {return 0;}" > foo.c
echo "CONFIG_FOO=y" > build/tup.config
touch sub/bar.c
update

check_exist build/foo.o build/sub/bar.o build/prog
check_not_exist foo.o sub/bar.o prog

mv sub newsub
update_fail_msg "Expected node 'sub' to be in directory"

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o newsub/*.o |> gcc %f -o %o |> prog
HERE
update

check_exist build/foo.o build/newsub/bar.o build/prog
check_not_exist build/sub

stop_monitor
eotup
