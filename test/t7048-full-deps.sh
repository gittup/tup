#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2022  Mike Shal <marfey@gmail.com>
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

# Same as t4062, but with the monitor
. ./tup.sh
check_monitor_supported
check_tup_suid

set_full_deps

monitor

cp ../testTupfile.tup Tupfile

echo "int main(void) {}" > foo.c
update
sym_check foo.o main

echo "void foo2(void) {}" >> foo.c
update
sym_check foo.o main foo2

path="/usr/bin/"
filename="gcc"
case $tupos in
	CYGWIN*)
		path="c:\\MinGW\\bin\\"
		filename="gcc.exe"
		;;
esac

tup_dep_exist $path $filename . 'gcc -c foo.c -o foo.o'

tup fake_mtime $path$filename 5
if [ "$(tup | grep -c 'gcc -c')" != 1 ]; then
	echo "Changing timestamp on /usr/bin/gcc should have caused foo.c to re-compile." 1>&2
	exit 1
fi

eotup
