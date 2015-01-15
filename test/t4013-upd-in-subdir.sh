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

# Apparently I have a bug where I run the updater in a subdirectory and it
# gets confused and thinks it's writing to the wrong file.

. ./tup.sh

tmkdir a
tmkdir a/b
cp ../testTupfile.tup a/b/Tupfile

echo "int main(void) {}" > a/b/foo.c
tup touch a/b/foo.c a/b/Tupfile
update
sym_check a/b/prog.exe main

tup touch a/b/foo.c
cd a
update

eotup
