#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2015  Mike Shal <marfey@gmail.com>
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

# Make sure the /-files go away if they are no longer needed.
. ./tup.sh
check_tup_suid

set_full_deps

cp ../testTupfile.tup Tupfile

echo "int main(void) {}" > foo.c
tup touch foo.c
update
sym_check foo.o main

path="/usr/bin/"
filename="gcc"
case $tupos in
	CYGWIN*)
		path="c:\\MinGW\\bin\\"
		filename="gcc.exe"
		;;
esac

tup_object_exist $path $filename

rm Tupfile
tup rm Tupfile
update
tup_object_no_exist $path $filename

eotup
