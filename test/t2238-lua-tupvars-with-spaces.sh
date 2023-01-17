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

# Make sure setting a variable in a regular Tupfile with a space will result in
# multiple table entries rather than a single entry with a space, since
# variables in a Tupfile are space-separated.

. ./tup.sh
cat > Tupfile << HERE
files = foo.c bar.c
other_files += baz.c boo.c
include rules.lua
HERE
cat > rules.lua << HERE
files += other_files
tup.foreach_rule(files, 'gcc -c %f -o %o', '%B.o')
HERE
touch foo.c bar.c baz.c boo.c
update

eotup
