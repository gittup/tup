#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2021  Mike Shal <marfey@gmail.com>
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

# Make sure all forms of sqlite globbing work.

. ./tup.sh
cat > Tupfile << HERE
: foreach *.c [123].d ?.e |> cat %f |>
HERE
tup touch foo.c bar.c 1.d 2.d 3.d 5.e Tupfile
tup touch boo.cc 4.d 52.e
update
for i in foo.c bar.c 1.d 2.d 3.d 5.e; do
	check_exist $i
	tup_dep_exist . $i . "cat $i"
done
for i in boo.cc 4.d 52.e; do
	check_exist $i
	tup_object_no_exist . "cat $i"
done

eotup
