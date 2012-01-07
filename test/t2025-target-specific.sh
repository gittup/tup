#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2012  Mike Shal <marfey@gmail.com>
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

# See if target specific variables work. Also make sure we can't post-set
# variables after a rule.

. ./tup.sh
cat > Tupfile << HERE
var_foo = hey
var_bar = yo
srcs = *.c
: foreach \$(srcs) |> gcc -DBLAH=\$(var_%B) -c %f -o %o |> %B.o
var_bar = BREAK
HERE

tup touch Tupfile foo.c bar.c
tup parse
tup_object_exist . "gcc -DBLAH=hey -c foo.c -o foo.o"
tup_object_exist . "gcc -DBLAH=yo -c bar.c -o bar.o"

eotup
