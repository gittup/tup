#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2018  Mike Shal <marfey@gmail.com>
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

# Try to include a header that was auto-generated from another command. Assume
# we correctly have the dependency specified in the rule the first time, and
# then later remove it. We should get yelled at.
. ./tup.sh
single_threaded
cat > Tupfile << HERE
: foo.h.in |> cp %f %o |> %B
: foreach *.c | foo.h |> gcc -c %f -o %o |> %B.o
HERE

echo "#define FOO 3" > foo.h.in
cat > foo.c << HERE
#include "foo.h"
int main(void) {return FOO;}
HERE
tup touch foo.c foo.h.in Tupfile
update

cat > Tupfile << HERE
: foo.h.in |> cp %f %o |> %B
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
tup touch Tupfile
update_fail_msg "Missing input dependency"

eotup
