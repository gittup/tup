#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2022  Mike Shal <marfey@gmail.com>
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

# If a rule lists an output file but doesn't actually create the file, the
# dependency can be lost. This can cause an orphaned output file (if the file
# was previously created by a different command).

. ./tup.sh

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE

echo "int main(void) {}" > foo.c
update
check_exist foo.o

cat > Tupfile << HERE
: foreach *.c |> echo gcc -c %f -o %o |> %B.o
HERE

update_fail_msg "Expected to write to file 'foo.o'"

eotup
