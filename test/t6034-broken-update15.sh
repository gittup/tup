#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2009-2024  Mike Shal <marfey@gmail.com>
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

# Continuation of t6033. So I can't accurately discover circular dependencies
# just by checking which nodes are on plist or not. The problem is if A->B with
# a sticky link, then B won't be expanded immediately. Therefore if somewhere
# down the chain B->...->A, I won't know. Further, if C->B is a normal link,
# then B will be expanded *later*, after A is already finished. The only way
# to detect circular dependencies while building the graph would be to always
# load the entire partial DAG (even nodes that won't be executed, because the
# only incoming links are sticky). Not sure if that would be worthwhile.

. ./tup.sh
cat > Tupfile << HERE
: foreach foo.c | foo.h |> gcc -c %f -o %o |> foo.o
HERE
touch foo.c foo.h
update

touch foo.h foo.c
update

eotup
