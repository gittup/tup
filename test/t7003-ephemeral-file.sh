#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2008-2015  Mike Shal <marfey@gmail.com>
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
monitor
cp ../testTupfile.tup Tupfile

echo "int main(void) {return 0;}" > foo.c
update
tup_object_exist . foo.o prog.exe

# See if we can create and destroy a file almost immediately after and not have
# the monitor put the directory in create.
echo "Check 1"
touch bar.c
touch bar.c
touch bar.c
rm bar.c
check_empty_tupdirs

# See if we can do it when moving a file to a new file
echo "Check 2"
touch bar.c
mv bar.c newbar.c
rm newbar.c
check_empty_tupdirs

eotup
