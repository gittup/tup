#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2018-2023  Mike Shal <marfey@gmail.com>
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

# Make sure a basic clang setup works. We don't do linking since that's
# different on Windows.
. ./tup.sh

cat > Tupfile << HERE
: foreach *.c |> clang -c %f -o %o |> %B.o
HERE

echo "int main(void) {}" > foo.c
echo '#include "bar.h"' > bar.c
touch bar.h
update

tup_dep_exist . bar.h . 'clang -c bar.c -o bar.o'
tup_dep_no_exist . bar.h . 'clang -c foo.c -o foo.o'

eotup
