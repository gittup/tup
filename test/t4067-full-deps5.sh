#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2012-2017  Mike Shal <marfey@gmail.com>
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

# Make sure we rebuild everything if updater.full_deps is enabled so that we can pick up
# new dependencies. If it is disabled, we don't need to rebuild since we can just delete
# the whole "/" tree.
. ./tup.sh
check_tup_suid

path="/usr/bin/"
filename="gcc"
case $tupos in
	CYGWIN*)
		if which gcc | grep MinGW > /dev/null; then
			path="c:\\MinGW\\bin\\"
		else
			path="c:\\cygwin64\\bin\\"
		fi
		filename="gcc.exe"
		;;
esac

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
tup touch Tupfile foo.c bar.c
update

tup_object_no_exist $path $filename

set_full_deps
if ! tup upd | grep 'gcc -c' | wc -l | grep 2 > /dev/null; then
	echo "Error: All files should have been recompiled when updater.full_deps was set." 1>&2
	exit 1
fi
tup_object_exist $path $filename

clear_full_deps
if ! tup upd | grep 'gcc -c' | wc -l | grep 0 > /dev/null; then
	echo "Error: No files should have been recompiled when updater.full_deps was cleared." 1>&2
	exit 1
fi
tup_object_no_exist $path $filename

eotup
