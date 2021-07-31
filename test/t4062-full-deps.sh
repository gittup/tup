#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2021  Mike Shal <marfey@gmail.com>
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

# See if we can get a dependency on an external file like /usr/bin/gcc, and update when
# that changes.
. ./tup.sh
check_tup_suid

set_full_deps

cp ../testTupfile.tup Tupfile

echo "int main(void) {}" > foo.c
tup parse > .output.txt 2>&1
gitignore_bad / .output.txt
update
sym_check foo.o main

echo "void foo2(void) {}" >> foo.c
update
sym_check foo.o main foo2

path="/usr/bin/"
filename="gcc"
case $tupos in
	CYGWIN*)
		if which gcc | grep MinGW  > /dev/null; then
			path="c:\\MinGW\\mingw32\\bin\\"
		else
			path="c:\\cygwin64\\bin\\"
		fi
		filename="gcc.exe"
		;;
esac

tup_dep_exist $path $filename . 'gcc -c foo.c -o foo.o'

tup fake_mtime $path$filename 5
if tup upd | grep 'gcc -c' | wc -l | grep 1 > /dev/null; then
	:
else
	echo "Changing timestamp on /usr/bin/gcc should have caused foo.c to re-compile." 1>&2
	exit 1
fi

if [ "`tup entry $path$filename`" = "" ]; then
	echo "Error: tup entry failed on $path$filename" 1>&2
	exit 1
fi

eotup
