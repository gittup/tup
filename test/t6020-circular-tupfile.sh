#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2015  Mike Shal <marfey@gmail.com>
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

# See if we have a Tupfile in '.' that uses files from 'foo', and a Tupfile in
# 'foo' that uses files from '.' that we get yelled at. This is what we in the
# biz call a "circular dependency".

. ./tup.sh
cat > Tupfile << HERE
: foo/*.o |> gcc %f -o %o |> prog
HERE

tmkdir foo
cat > foo/Tupfile << HERE
: foreach ../*.c |> gcc -c %f -o %o |> %B.o
HERE

cat > foo/ok.c << HERE
int main(void) {return 0;}
HERE

tup touch Tupfile foo/Tupfile foo/ok.c
update_fail

eotup
