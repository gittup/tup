#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2010-2020  Mike Shal <marfey@gmail.com>
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

# Try out 'tup todo'

. ./tup.sh
cp ../testTupfile.tup Tupfile

echo "int main(void) {}" > foo.c
echo "void bar1(void) {}" > bar.c
tup touch foo.c bar.c
update

tup touch Tupfile
tup todo | grep 'Tup phase 2'
parse

tup touch foo.c
tup todo | grep 'Tup phase 3.* 2 commands'
tup upd

tup todo | grep 'Everything is up-to-date'

eotup
