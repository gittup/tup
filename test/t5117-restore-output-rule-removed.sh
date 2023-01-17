#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2022-2023  Mike Shal <marfey@gmail.com>
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

# If we create a file using a ^o rule, then the rule to create that file fails
# (so the file gets restored by the updater rather than removed), then remove
# the rule, the file should be gone.
. ./tup.sh
cat > Tupfile << HERE
: foreach *.c |> ^o^ gcc -c %f -o %o |> %B.o
HERE

echo "int main(void) {return 0;}" > foo.c
update
check_exist foo.o

echo "int main(void) {return 0;} borf" > foo.c
update_fail_msg 'expected'

cat > Tupfile << HERE
HERE
update
check_not_exist foo.o

eotup
