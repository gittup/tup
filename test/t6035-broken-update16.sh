#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2016  Mike Shal <marfey@gmail.com>
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

# If I run with -j2, and have one file successfully compile while another
# stops the build, then change a config option to remove both files, the file
# that was successfully built will stick around as if the generated file
# became a normal file. This file should be removed.
#
# This would happen because after the updater runs a command, it puts all of
# the outputs in the modify list. That is done so the updater can pick up where
# it left off in case of an error or CTRL-c. In that case, the fact that the
# node is in the modify list doesn't actually indicate that it was overwritten
# by the user.
#
# Note this test won't always fail since it relies on the scheduling of -j2,
# but it is pretty consistent when the bug exists.

. ./tup.sh
cat > Tupfile << HERE
obj-@(OK) += foo.c
obj-@(OK) += bar.c
: foreach \$(obj-y) |> gcc -c %f -o %o |> %B.o
HERE
echo 'bork' > bar.c
tup touch foo.c bar.c Tupfile
varsetall OK=y
update_fail -j2

check_exist foo.o
check_not_exist bar.o

varsetall OK=n
update

check_not_exist foo.o
check_not_exist bar.o

tup_object_no_exist . foo.o
tup_object_no_exist . bar.o

eotup
